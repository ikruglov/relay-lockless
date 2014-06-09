#include <sys/types.h>
#include <sys/socket.h>

#include "ev_cb.h"
#include "common.h"

context_t* init_context() {
    context_t* ctx = calloc(1, sizeof(context_t));
    if (!ctx) ERRPX("Failed to calloc");

    ctx->list = list_init();
    return ctx;
}

io_server_watcher_t* new_io_server_watcher(struct ev_loop* loop, io_watcher_cb cb, socket_t* sock) {
    assert(loop);
    assert(sock);
    assert(sock->socket >= 0);

    context_t* ctx = (context_t*) ev_userdata(loop);
    io_server_watcher_t* isw = &ctx->servers[ctx->servers_cnt++];

    ev_io_init(&isw->io, cb, sock->socket, EV_READ);
    isw->offset = 0;
    isw->size = 0;

    ev_io_start(loop, &isw->io);
    return isw;
}

void udp_server_cb(struct ev_loop* loop, ev_io* w, int revents) {
    io_server_watcher_t* isw = (io_server_watcher_t*) w;
    context_t* ctx = (context_t*) ev_userdata(loop);

    ssize_t rlen = recv(w->fd, isw->buf, sizeof(isw->buf), 0);

    if (rlen > 0) {
        // normal workflow
        list_item_t* item = list_enqueue_new(ctx->list, rlen);
        memcpy(item->data, isw->buf, rlen);

        //_D("UDP packet received: %d", rlen);
    } else if (rlen == 0 || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
        ev_io_stop(loop, w);         // stop watcher
        ev_break(loop, EVBREAK_ALL); // exit from ev_run
        close(w->fd);

        if (rlen == 0) _D("shutdown %d", w->fd);
        else _D("recv() returned error [fd:%d] %s", w->fd, strerror(errno));
    }
}

io_client_watcher_t* new_io_client_watcher(struct ev_loop* loop, io_watcher_cb cb, socket_t* sock) {
    assert(loop);
    assert(sock);
    assert(sock->socket >= 0);

    context_t* ctx = (context_t*) ev_userdata(loop);
    io_client_watcher_t* icw = &ctx->clients[ctx->clients_cnt++];

    ev_io_init(&icw->io, cb, sock->socket, EV_WRITE);

    icw->item = ctx->list->head;
    icw->connected = 0;
    icw->sock = sock;
    icw->offset = 0;
    icw->size = 0;

    ev_io_start(loop, &icw->io);
    return icw;
}

void try_to_connect(io_client_watcher_t* icw) {
    assert(icw);

    socket_t* sock = icw->sock;
    int ret = connect(sock->socket, (struct sockaddr *) &sock->in, sizeof(sock->in));
    if (ret == 0) {
        icw->connected = 1;
    } else if (ret == -1 && errno != EINPROGRESS) {
        _D("connect() failed: %s", strerror(errno)); // TODO
    }
}

void tcp_client_cb(struct ev_loop* loop, ev_io* w, int revents) {
    io_client_watcher_t* icw = (io_client_watcher_t*) w;

    if (!icw->connected) {
        // no connected and callback is called
        // means that result of connect() should
        // be investivated
        int err = 0;
        socklen_t len = sizeof(err);
        getsockopt(w->fd, SOL_SOCKET, SO_ERROR, &err, &len);
        if (err) {
            _D("getsockopt() tells that connect() failed [fd:%d] [%s] %s",
               w->fd, icw->sock->to_string, strerror(err));

            ev_io_stop(loop, w);
            return;
        }

        _D("connected to %s", icw->sock->to_string);
        icw->connected = 1;
    } else {
        // normal send logic
        // if we fail send an item due to any reason
        // skip it at rely on background process
        // to dump it do disk
        list_item_t* item = icw->item;

        // data of current item has been send, advance to next one
        if (icw->offset >= icw->size) {
            if (!item->next) {
                // nothing to pick up from queue
                // temporary stop watcher
                ev_io_stop(loop, w);
                return;
            }

            icw->item = item->next;
            item = icw->item;

            icw->size = item->size;
            icw->offset = 0;

            // ignore empty items
            if (icw->size == 0) return;
        }

        ssize_t wlen = send(w->fd, item->data + icw->offset, icw->size - icw->offset, 0);
        if (wlen > 0) {
            icw->offset += wlen; // data was sent, normal workflow
        } else if (wlen != 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
            // error condition, disconnect from socket
            _D("send() returned error [fd:%d] [%s] %s",
               w->fd, icw->sock->to_string, strerror(errno));

            ev_io_stop(loop, w);
            icw->connected = 0;
            close(w->fd);

            //dump elemnt to dist
            //TODO
        }
    }
}

void wakeup_clients(struct ev_loop* loop, ev_timer* w, int revents) {
    context_t* ctx = (context_t*) ev_userdata(loop);
    for (int i = 0; i < ctx->clients_cnt; ++i) {
        io_client_watcher_t* icw = &ctx->clients[i];
        if (!ev_is_active(&icw->io) && icw->connected) {
            ev_io_start(loop, &icw->io);
        }
    }
}

void reconnect_clients_cb(struct ev_loop* loop, ev_timer* w, int revents) {
    context_t* ctx = (context_t*) ev_userdata(loop);
    for (int i = 0; i < ctx->clients_cnt; ++i) {
        io_client_watcher_t* icw = &ctx->clients[i];
        if (!ev_is_active(&icw->io) && !icw->connected) {
            _D("try reconnect to %s", icw->sock->to_string);

            setup_socket(icw->sock); //TODO remove ERRPX
            try_to_connect(icw);

            ev_io_set(&icw->io, icw->sock->socket, EV_WRITE);
            ev_io_start(loop, &icw->io);

            icw->offset = 0;
            icw->size = 0;
        }
    }
}
