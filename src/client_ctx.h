#ifndef __CLIENT_CTX_H__
#define __CLIENT_CTX_H__

#include "ev.h"
#include "net.h"
#include "list.h"

#define MAX_CLIENT_CONNECTIONS 1024

#define SET_LIST_ITEM(__w, __i)           ATOMIC_WRITE((__w)->item, (__i))
#define GET_LIST_ITEM(__w)                ATOMIC_READ ((__w)->item)
#define SET_CONTEXT_CLIENT(__c, __i, __w) ATOMIC_WRITE((__c)->clients[__i], __w)
#define GET_CONTEXT_CLIENT(__c, __i)      ATOMIC_READ ((__c)->clients[__i])

struct _io_client_watcher {
    ev_io io;
    size_t id;         // id inside _context->clients
    uint32_t size;     // size of item->data, 4GB at most
    uint32_t offset;   // offset inside item->data, 4GB at most
    list_item_t* item; // current item in g_list
    socket_t* sock;    // watcher owns socket object
#ifdef DOSTATS
    uint64_t bytes;
    uint64_t processed;
#endif
    int connected;
};

typedef struct _io_client_watcher io_client_watcher_t;

struct _client_context {
    list_t* list;            // ptr to list_t in server_ctx
    struct ev_loop* loop;    // libev loop
    ev_async stop_loop;      // signal to interrupt loop
    ev_async wakeup_clients; // priority 1
    ev_timer reconnect_clients;
    size_t active_clients, total_clients;
    io_client_watcher_t* clients[MAX_CLIENT_CONNECTIONS]; // priority 0
};

typedef struct _client_context client_ctx_t;
client_ctx_t* init_client_context();
void free_client_context(client_ctx_t* ctx);

typedef void (io_watcher_cb)(struct ev_loop* loop, ev_io *w, int revents);
io_client_watcher_t* init_io_client_watcher(client_ctx_t* ctx, io_watcher_cb cb, socket_t* sock);
void free_client_watcher(client_ctx_t* ctx, io_client_watcher_t* watcher);

void tcp_client_cb(struct ev_loop* loop, ev_io* w, int revents);
void wakeup_clients_cb(struct ev_loop* loop, ev_async* w, int revents);
void reconnect_clients_cb(struct ev_loop* loop, ev_timer* w, int revents);

int try_connect(io_client_watcher_t* icw);

#endif
