#include "common.h"
#include "server_ctx.h"

#define _DN(fmt, obj, arg...) _D(fmt " [%s]", ##arg, obj->sock->to_string)
#define _EN(fmt, obj, arg...) _D(fmt " [%s]: %s", ##arg, obj->sock->to_string, errno ? strerror(errno) : "undefined error")

server_ctx_t* init_server_context(client_ctx_t* client_ctx) {
    assert(client_ctx);
    server_ctx_t* ctx = calloc_or_die(1, sizeof(server_ctx_t));

    ctx->loop = ev_loop_new(0);
    ctx->client_ctx = client_ctx;
    ctx->list = list_init();

    client_ctx->list = ctx->list;

    ev_set_userdata(ctx->loop, ctx);
    return ctx;
}

void free_server_context(server_ctx_t* ctx) {
    if (!ctx) return;

    for (size_t i = 0; i < MAX_SERVER_CONNECTIONS; ++i) {
        if (ctx->servers[i])
            free(ctx->servers[i]);
    }

    ev_loop_destroy(ctx->loop);
    list_free(ctx->list);
    free(ctx);
}

io_server_watcher_t* init_io_server_watcher(server_ctx_t* ctx, io_watcher_cb cb, socket_t* sock) {
    assert(ctx);
    assert(sock);
    assert(sock->socket >= 0);

    io_server_watcher_t* isw = NULL;
    struct ev_loop* loop = ctx->loop;

    for (int i = 0; i < MAX_SERVER_CONNECTIONS; ++i) {
        if (!ctx->servers[i]) {
            isw = malloc_or_die(sizeof(io_server_watcher_t));
            isw->id = i;

            ctx->servers[i] = isw;
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

void free_server_watcher(server_ctx_t* ctx, io_server_watcher_t* watcher) {
    if (!watcher) return;

    struct ev_loop* loop = ctx->loop;
    ev_io_stop(loop, &watcher->io);
    close(watcher->sock->socket);

    ctx->servers[watcher->id] = NULL;

    free(watcher->sock);
    free(watcher);
}

void udp_server_cb(struct ev_loop* loop, ev_io* w, int revents) {
    server_ctx_t* ctx = (server_ctx_t*) ev_userdata(loop);
    io_server_watcher_t* isw = (io_server_watcher_t*) w;

    ssize_t rlen = recv(w->fd, isw->buf, sizeof(isw->buf), 0);

    if (rlen > 0) {
        // normal workflow
        //_DN("UDP packet received: %d", isw, isw->size);
        isw->size = rlen; // to have correct sizeof(isw->size)
        
        if (isw->size > MAX_MESSAGE_SIZE) {
            _DN("UDP message size %d is bigger then %d", isw, isw->size, MAX_MESSAGE_SIZE);
            goto udp_server_cb_error;
        }

        // enqueue new item in list
        // TODO fix data race with item->data, tsan is silient about it!
        list_item_t* item = list_new(rlen + sizeof(isw->size));
        memcpy(item->data, &isw->size, sizeof(isw->size));
        memcpy(item->data + sizeof(isw->size), isw->buf, rlen);
        list_enqueue(ctx->list, item);

        // wakeup stopped clients
        client_ctx_t* cctx = ctx->client_ctx;
        if (cctx) ev_async_send(cctx->loop, &cctx->wakeup_clients);

#if DOSTATS
        ATOMIC_INCREMENT(isw->processed);
        ATOMIC_INCREASE(isw->bytes, isw->size);
#endif

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
    free_server_watcher(ctx, isw);
}

void tcp_accept_cb(struct ev_loop* loop, ev_io* w, int revents) {
    server_ctx_t* ctx = (server_ctx_t*) ev_userdata(loop);
    io_server_watcher_t* isw = (io_server_watcher_t*) w;

    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    int client = accept(w->fd, (struct sockaddr*) &addr, &addrlen);
    //TODO verify addrlen == addrlen after call

    if (client >= 0) {
        socket_t* sock = socketize_sockaddr(&addr);
        sock->socket = client;
        init_io_server_watcher(ctx, tcp_server_cb, sock);
    } else if (errno != EINTR && errno != EAGAIN) {
        _DN("accept() returned error", isw);
        goto tcp_accept_cb_error;
    }

    return;

tcp_accept_cb_error:
    free_server_watcher(ctx, isw);
}

static char* buf = NULL;
static size_t buf_size = MAX_MESSAGE_SIZE;
void tcp_server_cb(struct ev_loop* loop, ev_io* w, int revents) {
    server_ctx_t* ctx = (server_ctx_t*) ev_userdata(loop);
    io_server_watcher_t* isw = (io_server_watcher_t*) w;

    if (!buf)
        buf = malloc(buf_size);

    char* pbuf = buf;
    ssize_t rlen = recv(w->fd, pbuf, buf_size, 0);

    if (rlen > 0) {
        while (rlen > 0) {
            if (isw->size == 0 && rlen + isw->offset >= sizeof(isw->size)) {
                memcpy((char*) &isw->size, isw->buf, isw->offset);
                memcpy((char*) &isw->size + isw->offset, pbuf, sizeof(isw->size) - isw->offset);

#ifdef DEBUG
                if (isw->size > MAX_MESSAGE_SIZE) {
                    _DN("TCP message size %d is bigger then %d", isw, isw->size, MAX_MESSAGE_SIZE);
                    exit(1);
                    goto tcp_server_cb_error;
                }
#endif
            }

            size_t total_size = isw->size + sizeof(isw->size);
            if (rlen + isw->offset >= total_size) {
                //_DN("TCP packet received: %d", isw, isw->size);
                list_item_t* item = list_enqueue_new(ctx->list, total_size);
                memcpy(item->data, isw->buf, isw->offset);
                memcpy(item->data + isw->offset, pbuf, total_size - isw->offset);

                pbuf += total_size - isw->offset;
                rlen -= total_size - isw->offset;

#if DOSTATS
                ATOMIC_INCREMENT(isw->processed);
                ATOMIC_INCREASE(isw->bytes, isw->size);
#endif

                isw->offset = 0;
                isw->size = 0;
            } else {
                break;
            }
        }

        //isw->size = 0;
        memcpy(isw->buf + isw->offset, pbuf, rlen);
        isw->offset += rlen;
    } else if (rlen == 0) {
        _DN("shutdown", isw);
        goto tcp_server_cb_error;
    } else if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
        _EN("recv() returned error", isw);
        goto tcp_server_cb_error;
    }

    return;

tcp_server_cb_error:
    free_server_watcher(ctx, isw);
}
