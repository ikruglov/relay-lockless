#include <sys/types.h>
#include <sys/socket.h>

#include "ev_cb.h"
#include "common.h"

#define _DN(fmt, obj, arg...) _D(fmt " [%s]", ##arg, obj->sock->to_string)
#define _EN(fmt, obj, arg...) _D(fmt " [%s]: %s", ##arg, obj->sock->to_string, errno ? strerror(errno) : "undefined error")

context_t* init_context() {
    context_t* ctx = calloc_or_die(1, sizeof(context_t));

    ctx->list = list_init();

    ctx->wakeup_clients = malloc_or_die(sizeof(ev_async));
    ev_async_init(ctx->wakeup_clients, wakeup_clients_cb);
    ev_set_priority(ctx->wakeup_clients, 1);

    return ctx;
}

void free_context(context_t* ctx) {
    if (!ctx) return;

    for (size_t i = 0; i < MAX_CLIENTS; ++i) {
        io_client_watcher_t* icw = ctx->clients[i];
        if (icw) free(icw);
    }

    for (size_t i = 0; i < MAX_SERVERS; ++i) {
        io_server_watcher_t* isw = ctx->servers[i];
        if (isw) free(isw);
    }

    if (ctx->wakeup_clients)
        free(ctx->wakeup_clients);

    free(ctx);
}

io_server_watcher_t* new_io_server_watcher(struct ev_loop* loop, io_watcher_cb cb, socket_t* sock) {
    assert(loop);
    assert(sock);
    assert(sock->socket >= 0);

    io_server_watcher_t* isw = NULL;
    context_t* ctx = (context_t*) ev_userdata(loop);

    for (size_t i = 0; i < MAX_SERVERS; ++i) {
        if (!ctx->servers[i]) {
            isw = malloc_or_die(sizeof(io_server_watcher_t));
            ctx->servers[i] = isw;
            isw->id = i;
            break;
        }
    }

    if (!isw) {
        _D("Failed to create new server watcher");
        return NULL;
    }

    ev_io_init(&isw->io, cb, sock->socket, EV_READ);
    isw->sock = sock;
    isw->offset = 0;
    isw->size = 0;

    // server events should be handled first
    ev_set_priority(&isw->io, 2);
    ev_io_start(loop, &isw->io);
    return isw;
}

void tcp_accept_cb(struct ev_loop* loop, ev_io* w, int revents) {
    context_t* ctx = (context_t*) ev_userdata(loop);
    io_server_watcher_t* isw = (io_server_watcher_t*) w;

    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    int client = accept(w->fd, (struct sockaddr*) &addr, &addrlen);
    //TODO verify addrlen == addrlen after call

    if (client >= 0) {
        socket_t* sock = socketize_sockaddr(&addr);
        sock->socket = client;
        new_io_server_watcher(loop, tcp_server_cb, sock);
    } else if (errno != EINTR && errno != EAGAIN) {
        _DN("accept() returned error", isw);
        goto tcp_accept_cb_error;
    }

    return;

tcp_accept_cb_error:
    // stop watcher
    ev_io_stop(loop, w);
    close(w->fd);

    // free watcher
    ctx->servers[isw->id] = NULL;
    free(isw);
}

void tcp_server_cb(struct ev_loop* loop, ev_io* w, int revents) {
    context_t* ctx = (context_t*) ev_userdata(loop);
    io_server_watcher_t* isw = (io_server_watcher_t*) w;

    // if isw->size empty, it means that new message is expected
    // read its size first, body afterwards
    ssize_t rlen = isw->size == 0
                 ? recv(w->fd, isw->buf + isw->offset, sizeof(isw->size) - isw->offset, 0)
                 : recv(w->fd, isw->buf + isw->offset, isw->size - isw->offset, 0);

    if (rlen > 0) {
        isw->offset += rlen;
        if (isw->size == 0 && isw->offset >= sizeof(isw->size)) {
            memcpy(&isw->size, isw->buf, sizeof(isw->size));
            isw->size += sizeof(isw->size);

            if (isw->size > MAX_MESSAGE_SIZE) {
                _DN("TCP message size %d is bigger then %d", isw, isw->size, MAX_MESSAGE_SIZE);
                goto tcp_server_cb_error;
            }
        }

        if (isw->offset == isw->size) {
            // a message is fully read
            //_DN("TCP packet received %d", isw, isw->size);

            // enqueue new item in list
            list_item_t* item = list_enqueue_new(ctx->list, isw->size);
            memcpy(item->data, isw->buf, isw->size); // buf already has size in it

            // wakeup stopped clients
            ev_async_send(loop, ctx->wakeup_clients);

            isw->offset = 0;
            isw->size = 0;
        }
    } else if (rlen == 0) {
        _DN("shutdown", isw);
        goto tcp_server_cb_error;
    } else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
        _EN("recv() returned error", isw);
        goto tcp_server_cb_error;
    }

    return;

tcp_server_cb_error:
    // stop watcher
    ev_io_stop(loop, w);
    close(w->fd);

    // free watcher
    ctx->servers[isw->id] = NULL;
    free(isw);
}

void udp_server_cb(struct ev_loop* loop, ev_io* w, int revents) {
    io_server_watcher_t* isw = (io_server_watcher_t*) w;
    context_t* ctx = (context_t*) ev_userdata(loop);

    ssize_t rlen = recv(w->fd, isw->buf, sizeof(isw->buf), 0);

    if (rlen > 0) {
        // normal workflow
        isw->size = rlen;
        //_DN("UDP packet received: %d", isw, isw->size);
        if (isw->size > MAX_MESSAGE_SIZE) {
            _DN("UDP message size %d is bigger then %d", isw, isw->size, MAX_MESSAGE_SIZE);
            goto udp_server_cb_error;
        }

        // enqueue new item in list
        list_item_t* item = list_enqueue_new(ctx->list, rlen + sizeof(isw->size));
        memcpy(item->data, &isw->size, sizeof(isw->size));
        memcpy(item->data + sizeof(isw->size), isw->buf, rlen);

        // wakeup stopped clients
        ev_async_send(loop, ctx->wakeup_clients);

        isw->offset = 0;
        isw->size = 0;
    } else if (rlen == 0) {
        _DN("shutdown", isw);
        goto udp_server_cb_error;
    } else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
        _EN("recv() returned error", isw);
        goto udp_server_cb_error;
    }

    return;

udp_server_cb_error:
    //stop watcher
    ev_io_stop(loop, w);
    close(w->fd);

    // free watcher
    ctx->servers[isw->id] = NULL;
    free(isw);
}

io_client_watcher_t* new_io_client_watcher(struct ev_loop* loop, io_watcher_cb cb, socket_t* sock) {
    assert(loop);
    assert(sock);
    assert(sock->socket >= 0);

    io_client_watcher_t* icw = NULL;
    context_t* ctx = (context_t*) ev_userdata(loop);

    for (size_t i = 0; i < MAX_CLIENTS; ++i) {
        if (!ctx->clients[i]) {
            icw = malloc_or_die(sizeof(io_client_watcher_t));
            ctx->clients[i] = icw;
            icw->id = i;
            break;
        }
    }

    if (!icw) {
        _D("Failed to create new client watcher");
        return NULL;
    }

    ev_io_init(&icw->io, cb, sock->socket, EV_WRITE);

    icw->item = ctx->list->head;
    icw->connected = 0;
    icw->sock = sock;
    icw->offset = 0;
    icw->size = 0;

    ev_io_start(loop, &icw->io);
    return icw;
}

int try_connect(io_client_watcher_t* icw) {
    assert(icw);

    socket_t* sock = icw->sock;
    int ret = connect(sock->socket, (struct sockaddr *) &sock->in, sizeof(sock->in));
    if (ret == 0) {
        icw->connected = 1;
    } else if (ret == -1 && errno == EINPROGRESS) {
        ret = 0;
    } else {
        _EN("connect() failed", icw);
    }

    return ret;
}

void tcp_client_cb(struct ev_loop* loop, ev_io* w, int revents) {
    context_t* ctx = (context_t*) ev_userdata(loop);
    io_client_watcher_t* icw = (io_client_watcher_t*) w;

    if (!icw->connected) {
        // no connected and callback is called
        // means that result of connect() should be investigated
        int err = 0;
        socklen_t len = sizeof(err);
        getsockopt(w->fd, SOL_SOCKET, SO_ERROR, &err, &len);
        if (err) {
            _DN("getsockopt() tells that connect() failed: %s", icw, strerror(err));
            ev_io_stop(loop, w);
            return;
        }

        _DN("connected", icw);
        icw->connected = 1;
    } else {
        // normal send logic
        // if we fail send an item due to any reason
        // skip it and rely on background process to dump it do disk
        list_item_t* item = icw->item;

        // data of current item has been sent, advance to next one
        if (icw->offset >= icw->size) {
            if (!item->next) {
                // nothing to pick up from queue, temporary stop watcher
                // and start ev_async wakeup_clients watcher
                ev_io_stop(loop, w);
                ev_async_start(loop, ctx->wakeup_clients);
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
            _EN("send() returned error", icw);
            ev_io_stop(loop, w);
            icw->connected = 0;
            close(w->fd);

            icw->offset = 0;
            icw->size = 0;

            //dump element to disk
            //TODO
        }
    }

    //TODO cleanup client
}

// idea of having another async cb for waking up clients
// rely on fact that several async events (from different server watchers)
// could be aggregated in single one leading to less of work to do
void wakeup_clients_cb(struct ev_loop* loop, ev_async* w, int revents) {
    context_t* ctx = (context_t*) ev_userdata(loop);
    for (size_t i = 0; i < MAX_CLIENTS; ++i) {
        io_client_watcher_t* icw = ctx->clients[i];
        if (icw && !ev_is_active(&icw->io) && icw->connected) {
            ev_io_start(loop, &icw->io);
        }
    }

    // all clients started
    // when any clients stops, it starts this one
    ev_async_stop(loop, w);
}

void reconnect_clients_cb(struct ev_loop* loop, ev_timer* w, int revents) {
    context_t* ctx = (context_t*) ev_userdata(loop);
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        io_client_watcher_t* icw = ctx->clients[i];
        if (icw && !ev_is_active(&icw->io) && !icw->connected) {
            _DN("try reconnect", icw);

            if (setup_socket(icw->sock, 0) || try_connect(icw))
                continue;

            ev_io_set(&icw->io, icw->sock->socket, EV_WRITE);
            ev_io_start(loop, &icw->io);

            icw->offset = 0;
            icw->size = 0;
        }
    }
}

void cleanup_list_cb(struct ev_loop* loop, ev_timer* w, int revents) {
    context_t* ctx = (context_t*) ev_userdata(loop);
    list_t* list = ctx->list;

    uint64_t min_id = (uint64_t) -1;
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        io_client_watcher_t* icw = ctx->clients[i];
        if (!icw) continue;

        //sanity check
        assert(icw->item);
        assert(icw->item->id != (uint64_t) -1);

        if (icw->item->id < min_id) {
            min_id = icw->item->id;
        }
    }

    size_t deleted = 0;
    while (list->head->id < min_id && list->head->next) {
        list_dequeue(list);
        ++deleted;
    }

    _D("cleanup_list_cb(): %zu deleted, list size: %zu", deleted, list_size(list));
}
