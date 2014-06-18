#include "common.h"
#include "client_ctx.h"

#define _DN(fmt, obj, arg...) _D(fmt " [%s]", ##arg, obj->sock->to_string)
#define _EN(fmt, obj, arg...) _D(fmt " [%s]: %s", ##arg, obj->sock->to_string, errno ? strerror(errno) : "undefined error")

static void stop_loop_cb(struct ev_loop* loop, ev_async* w, int revents);
static void free_client_watcher(client_ctx_t* ctx, io_client_watcher_t* watcher);

client_ctx_t* init_client_context() {
    client_ctx_t* ctx = calloc_or_die(1, sizeof(client_ctx_t));
    ctx->loop = ev_loop_new(EVFLAG_NOSIGMASK);
    ctx->active_clients = 0;
    ctx->total_clients = 0;
#ifdef DOSTATS
    ctx->processed = 0;
    ctx->bytes = 0;
#endif

    ev_async_init(&ctx->wakeup_clients, wakeup_clients_cb);
    ev_set_priority(&ctx->wakeup_clients, 1);

    ev_timer_init(&ctx->reconnect_clients, reconnect_clients_cb, 0, 1);
    ev_timer_start(ctx->loop, &ctx->reconnect_clients);

    ev_async_init(&ctx->stop_loop, stop_loop_cb);
    ev_async_start(ctx->loop, &ctx->stop_loop);

    ev_set_userdata(ctx->loop, ctx);
    return ctx;
}

void free_client_context(client_ctx_t* ctx) {
    if (!ctx) return;

    for (size_t i = 0; i < MAX_CLIENT_CONNECTIONS; ++i) {
        free_client_watcher(ctx, ctx->clients[i]);
    }

    ev_async_stop(ctx->loop, &ctx->stop_loop);
    ev_async_stop(ctx->loop, &ctx->wakeup_clients);
    ev_timer_stop(ctx->loop, &ctx->reconnect_clients);

    ev_loop_destroy(ctx->loop);
    free(ctx);
}

io_client_watcher_t* init_io_client_watcher(client_ctx_t* ctx, io_watcher_cb cb, socket_t* sock) {
    assert(ctx);
    assert(sock);
    assert(sock->socket >= 0);

    io_client_watcher_t* icw = NULL;
    struct ev_loop* loop = ctx->loop;

    for (int i = 0; i < MAX_CLIENT_CONNECTIONS; ++i) {
        if (!GET_CONTEXT_CLIENT(ctx, i)) {
            icw = malloc_or_die(sizeof(io_client_watcher_t));
            icw->id = i; // potential data race with free_client_watcher()

            SET_CONTEXT_CLIENT(ctx, i, icw);
            ctx->total_clients++;
            break;
        }
    }

    if (!icw) {
        _D("Failed to create new client watcher");
        return NULL;
    }

    SET_LIST_ITEM(icw, LIST_HEAD(ctx->list));

    icw->connected = 0;
    icw->sock = sock;
    icw->offset = 0;
    icw->size = 0;

    ev_io_init(&icw->io, cb, sock->socket, EV_WRITE);
    ev_set_priority(&icw->io, 0);
    ev_io_start(loop, &icw->io);
    ctx->active_clients++;

    return icw;
}

void free_client_watcher(client_ctx_t* ctx, io_client_watcher_t* watcher) {
    if (!watcher) return;

    ctx->active_clients--;
    struct ev_loop* loop = ctx->loop;
    ev_io_stop(loop, &watcher->io);
    close(watcher->sock->socket);

    // potential data race with init_io_client_watcher
    ctx->total_clients--;
    SET_CONTEXT_CLIENT(ctx, watcher->id, NULL);

    free(watcher->sock);
    free(watcher);
}

void tcp_client_cb(struct ev_loop* loop, ev_io* w, int revents) {
    client_ctx_t* ctx = (client_ctx_t*) ev_userdata(loop);
    io_client_watcher_t* icw = (io_client_watcher_t*) w;

    if (!icw->connected) {
        // no connected and callback is called
        // means that result of connect() should be investigated
        int err = 0;
        socklen_t len = sizeof(err);
        getsockopt(w->fd, SOL_SOCKET, SO_ERROR, &err, &len);
        if (err) {
            _DN("getsockopt() tells that connect() failed: %s", icw, strerror(err));
            ctx->active_clients--;
            ev_io_stop(loop, w);
            return;
        }

        _DN("connected", icw);
        icw->connected = 1;
    } else {
        // normal send logic
        // if we fail send an item due to any reason
        // skip it and rely on background process to dump it do disk
        list_item_t* item = GET_LIST_ITEM(icw);

        // data of current item has been sent, advance to next one
        if (icw->offset >= icw->size) {
            list_item_t* next = LIST_ITEM_NEXT(item);
            if (!next) {
                // nothing to pick up from queue, temporary stop watcher
                // and start ev_async wakeup_clients watcher
                ctx->active_clients--;
                ev_io_stop(loop, w);

                ev_async_start(loop, &ctx->wakeup_clients);
                return;
            }

            item = next;
            SET_LIST_ITEM(icw, next);
            icw->size = item->size;
            icw->offset = 0;

            // ignore empty items
            if (icw->size == 0) return;

#ifdef DOSTATS
            ATOMIC_INCREMENT(ctx->processed);
            ATOMIC_INCREASE(ctx->bytes, icw->size);
#endif
        }

        // TODO potential data race with item->data
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
    client_ctx_t* ctx = (client_ctx_t*) ev_userdata(loop);
    if (ctx->active_clients >= ctx->total_clients) return;

    for (int i = 0; i < MAX_CLIENT_CONNECTIONS; ++i) {
        io_client_watcher_t* icw = GET_CONTEXT_CLIENT(ctx, i);
        if (icw && !ev_is_active(&icw->io) && icw->connected) {
            ev_io_start(loop, &icw->io);
            ctx->active_clients++;
        }
    }

    // all clients started
    // when any clients stops, it starts this one
    ev_async_stop(loop, w);
}

void reconnect_clients_cb(struct ev_loop* loop, ev_timer* w, int revents) {
    client_ctx_t* ctx = (client_ctx_t*) ev_userdata(loop);

    for (int i = 0; i < MAX_CLIENT_CONNECTIONS; ++i) {
        io_client_watcher_t* icw = GET_CONTEXT_CLIENT(ctx, i);
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

void stop_loop_cb(struct ev_loop* loop, ev_async* w, int revents) {
    _D("Async signal received in client context. Break evloop");
    ev_break (loop, EVBREAK_ALL);
}
