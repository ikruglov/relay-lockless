#include "ev.h"
#include "net.h"
#include "list.h"
#include "common.h"

#define MAX_CLIENTS 1024
#define MAX_SERVERS 1024
#define MAX_MESSAGE_SIZE 65536

struct _io_client_watcher {
    ev_io io;
    size_t id;         // id inside _context->clients
    uint32_t size;     // size of item->data, 4GB at most
    uint32_t offset;   // offset inside item->data, 4GB at most
    list_item_t* item; // current item in g_list
    socket_t* sock;    // TODO free
    int connected;
};

struct _io_server_watcher {
    ev_io io;        // must be first
    size_t id;       // id inside _context->servers
    uint32_t size;   // 4GB at most
    uint32_t offset; // 4GB at most
    socket_t* sock;  // TODO free
    char buf[MAX_MESSAGE_SIZE];
};

struct _context {
    list_t* list;
    ev_async* wakeup_clients;
    struct _io_server_watcher* servers[MAX_SERVERS];
    struct _io_client_watcher* clients[MAX_CLIENTS];
};

typedef struct _context context_t;
typedef struct _io_server_watcher io_server_watcher_t;
typedef struct _io_client_watcher io_client_watcher_t;
typedef void (io_watcher_cb)(struct ev_loop *loop, ev_io *w, int revents);

context_t* init_context();
void free_context();

io_server_watcher_t* new_io_server_watcher(struct ev_loop* loop, io_watcher_cb cb, socket_t* sock);
io_client_watcher_t* new_io_client_watcher(struct ev_loop* loop, io_watcher_cb cb, socket_t* sock);

void udp_server_cb(struct ev_loop* loop, ev_io* w, int revents);
void tcp_accept_cb(struct ev_loop* loop, ev_io* w, int revents);
void tcp_server_cb(struct ev_loop* loop, ev_io* w, int revents);
void tcp_client_cb(struct ev_loop* loop, ev_io* w, int revents);

void cleanup_list_cb(struct ev_loop* loop, ev_timer* w, int revents);
void wakeup_clients_cb(struct ev_loop* loop, ev_async* w, int revents);
void reconnect_clients_cb(struct ev_loop* loop, ev_timer* w, int revents);

void wakeup_clients(struct ev_loop* loop);
int try_connect(io_client_watcher_t* icw);
