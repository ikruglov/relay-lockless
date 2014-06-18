#ifndef __SERVER_CTX_H__
#define __SERVER_CTX_H__

#include "ev.h"
#include "net.h"
#include "list.h"
#include "client_ctx.h"

#define MAX_MESSAGE_SIZE 65536
#define MAX_SERVER_CONNECTIONS 1024

struct _io_server_watcher {
    ev_io io;        // must be first
    size_t id;       // id inside _context->servers
    uint32_t size;   // 4GB at most
    uint32_t offset; // 4GB at most
    socket_t* sock;  // watcher owns socket object
    list_item_t* item;
};

typedef struct _io_server_watcher io_server_watcher_t;

struct _server_context {
    list_t* list;  // _server_context owns list
    struct ev_loop* loop;
    ev_async stop_loop; // signal to interrupt loop
    client_ctx_t* client_ctx;
#ifdef DOSTATS
    uint64_t bytes, processed;
#endif
    io_server_watcher_t* servers[MAX_SERVER_CONNECTIONS]; // priority 2
};

typedef struct _server_context server_ctx_t;
server_ctx_t* init_server_context(client_ctx_t* client_ctx);
void free_server_context(server_ctx_t* ctx);

io_server_watcher_t* init_io_server_watcher(server_ctx_t* ctx, io_watcher_cb cb, socket_t* sock);
void free_server_watcher(server_ctx_t* ctx, io_server_watcher_t* watcher);

void udp_server_cb(struct ev_loop* loop, ev_io* w, int revents);
void tcp_accept_cb(struct ev_loop* loop, ev_io* w, int revents);
void tcp_server_cb(struct ev_loop* loop, ev_io* w, int revents);

#endif
