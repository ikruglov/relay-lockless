#include <ev.h>
#include "net.h"
#include "list.h"

#define BUF_SIZE 65536

struct _io_client_watcher {
    ev_io io;
    size_t size;        // size of item->data, 4GB at most
    size_t offset;      // offset inside item->data, 4GB at most
    list_item_t* item;  // current item in g_list
    socket_t* sock;
    int connected;
};

struct _io_server_watcher {
    ev_io io;      // must be first
    size_t size;   // 4GB at most
    size_t offset; // 4GB at most
    char buf[BUF_SIZE];
};

struct _context {
    list_t* list;
    size_t servers_cnt;
    size_t clients_cnt;
    struct _io_server_watcher servers[8];
    struct _io_client_watcher clients[256];
};

typedef struct _context context_t;
typedef struct _io_server_watcher io_server_watcher_t;
typedef struct _io_client_watcher io_client_watcher_t;
typedef void (io_watcher_cb)(struct ev_loop *loop, ev_io *w, int revents);

context_t* init_context();

io_server_watcher_t* new_io_server_watcher(struct ev_loop* loop, io_watcher_cb cb, socket_t* sock);
io_client_watcher_t* new_io_client_watcher(struct ev_loop* loop, io_watcher_cb cb, socket_t* sock);

void udp_server_cb(struct ev_loop* loop, ev_io* w, int revents);
void tcp_client_cb(struct ev_loop* loop, ev_io* w, int revents);

void try_to_connect(io_client_watcher_t* icw);
void wakeup_clients(struct ev_loop* loop, ev_timer* w, int revents);
void reconnect_clients_cb(struct ev_loop* loop, ev_timer* w, int revents);

//void cleanup_list_cb(struct ev_loop *loop, ev_idle *w, int revents);
