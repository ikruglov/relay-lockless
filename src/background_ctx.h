#ifndef __BACKGROUND_CTX_H__
#define __BACKGROUND_CTX_H__

#include "ev.h"
#include "list.h"
#include "server_ctx.h"
#include "client_ctx.h"

struct _bg_context {
    struct ev_loop* loop;
    server_ctx_t* server_ctx;
    client_ctx_t* client_ctx;
    ev_async stop_loop;
    ev_timer cleanup_list;
#ifdef DOSTATS
    ev_timer stats_monitor;
#endif
};

typedef struct _bg_context bg_ctx_t;
bg_ctx_t* init_bg_context(server_ctx_t* server_ctx, client_ctx_t* client_ctx);
void free_bg_context(bg_ctx_t* ctx);

void cleanup_list_cb(struct ev_loop* loop, ev_timer* w, int revents);
void stats_monitor_cb(struct ev_loop* loop, ev_timer* w, int revents);
void signal_handler_cb(struct ev_loop* loop, ev_signal* w, int revents);

#endif
