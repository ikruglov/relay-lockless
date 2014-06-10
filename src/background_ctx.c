#include "list.h"
#include "common.h"
#include "background_ctx.h"

bg_ctx_t* init_bg_context(server_ctx_t* server_ctx, client_ctx_t* client_ctx) {
    assert(server_ctx);

    bg_ctx_t* ctx = calloc_or_die(1, sizeof(bg_ctx_t));

    ctx->loop = ev_default_loop(0);
    ctx->server_ctx = server_ctx;
    ctx->client_ctx = client_ctx;

    ev_timer_init(&ctx->cleanup_list, cleanup_list_cb, 0, 1);
    ev_timer_start(ctx->loop, &ctx->cleanup_list);

    ev_set_userdata(ctx->loop, ctx);
    return ctx;
}

void free_bg_context(bg_ctx_t* ctx) {
    if (!ctx) return;

    ev_timer_stop(ctx->loop, &ctx->cleanup_list);
    free(ctx);
}

void cleanup_list_cb(struct ev_loop* loop, ev_timer* w, int revents) {
    bg_ctx_t* ctx = (bg_ctx_t*) ev_userdata(loop);
    server_ctx_t* server_ctx = ctx->server_ctx;
    client_ctx_t* client_ctx = ctx->client_ctx;

    uint64_t min_id = (uint64_t) -1;
    for (int i = 0; i < MAX_CLIENT_CONNECTIONS; ++i) {
        io_client_watcher_t* icw = ATOMIC_READ(client_ctx->clients[i]);
        if (!icw) continue;

        list_item_t* item = get_list_item(icw);
        uint64_t id = list_item_id(item);

        if (id < min_id)
            min_id = id;
    }


    size_t deleted = 0;
    list_t* list = server_ctx->list;

    do {    
        list_item_t* head = list_head(list);
        if (list_item_id(head) >= min_id)
            break;

        ++deleted;
    } while (list_dequeue(list));

    //_D("cleanup_list_cb(): %zu deleted, list size: %zu", deleted, list_size(list));
}
