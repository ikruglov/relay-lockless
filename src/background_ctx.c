#include "list.h"
#include "common.h"
#include "background_ctx.h"

inline static void stop_loop_cb(struct ev_loop* loop, ev_async* w, int revents);

bg_ctx_t* init_bg_context(server_ctx_t* server_ctx, client_ctx_t* client_ctx) {
    assert(server_ctx);

    bg_ctx_t* ctx = calloc_or_die(1, sizeof(bg_ctx_t));

    ctx->loop = ev_loop_new(EVFLAG_NOSIGMASK);
    ctx->server_ctx = server_ctx;
    ctx->client_ctx = client_ctx;

    ev_async_init(&ctx->stop_loop, stop_loop_cb);
    ev_async_start(ctx->loop, &ctx->stop_loop);

    ev_timer_init(&ctx->cleanup_list, cleanup_list_cb, 0, 1);
    ev_timer_start(ctx->loop, &ctx->cleanup_list);

#ifdef DOSTATS
    ev_timer_init(&ctx->stats_monitor, stats_monitor_cb, 1, 1);
    ev_timer_start(ctx->loop, &ctx->stats_monitor);
#endif

    ev_set_userdata(ctx->loop, ctx);
    return ctx;
}

void free_bg_context(bg_ctx_t* ctx) {
    if (!ctx) return;

    ev_async_stop(ctx->loop, &ctx->stop_loop);
    ev_timer_stop(ctx->loop, &ctx->cleanup_list);
#ifdef DOSTATS
    ev_timer_stop(ctx->loop, &ctx->stats_monitor);
#endif

    free(ctx);
}

void cleanup_list_cb(struct ev_loop* loop, ev_timer* w, int revents) {
    bg_ctx_t* ctx = (bg_ctx_t*) ev_userdata(loop);
    server_ctx_t* server_ctx = ctx->server_ctx;
    client_ctx_t* client_ctx = ctx->client_ctx;

    uint64_t min_id = (uint64_t) -1;
    for (int i = 0; i < MAX_CLIENT_CONNECTIONS; ++i) {
        io_client_watcher_t* icw = GET_CONTEXT_CLIENT(client_ctx, i);
        if (!icw) continue;

        list_item_t* item = GET_LIST_ITEM(icw);
        uint64_t id = LIST_ITEM_ID(item);

        if (id < min_id)
            min_id = id;
    }

    size_t deleted = 0;
    list_t* list = server_ctx->list;

    do {    
        list_item_t* head = LIST_HEAD(list);
        if (LIST_ITEM_ID(head) >= min_id)
            break;

        ++deleted;
    } while (list_dequeue(list));

    //_D("cleanup_list_cb(): %zu deleted, list size: %zu", deleted, LIST_SIZE(list));
}

#ifdef DOSTATS
static uint64_t _elapsed_usec(struct timeval* start, struct timeval* end) {
    return ((end->tv_sec - start->tv_sec) * 1000000) + end->tv_usec - start->tv_usec;
}
#endif

void stats_monitor_cb(struct ev_loop* loop, ev_timer* w, int revents) {
#ifdef DOSTATS
    static struct timeval last = { 0, 0 };
    if (last.tv_sec == 0) {
        gettimeofday(&last, NULL);
        return;
    }

    static uint64_t last_servers_bytes = 0;
    static uint64_t last_clients_bytes = 0;
    static uint64_t last_servers_processed = 0;
    static uint64_t last_clients_processed = 0;

    bg_ctx_t* ctx = (bg_ctx_t*) ev_userdata(loop);
    server_ctx_t* server_ctx = ctx->server_ctx;
    client_ctx_t* client_ctx = ctx->client_ctx;

    uint64_t total_servers_bytes = 0;
    uint64_t total_servers_processed = 0;
    for (int i = 0; i < MAX_SERVER_CONNECTIONS; ++i) {
        io_server_watcher_t* isw = ATOMIC_READ(server_ctx->servers[i]);
        if (!isw) continue;

        total_servers_bytes     += ATOMIC_READ(isw->bytes);
        total_servers_processed += ATOMIC_READ(isw->processed);
    }

    size_t queue_lag = 0;
    size_t active_clients = 0;
    uint64_t total_clients_bytes = 0;
    uint64_t total_clients_processed = 0;
    for (int i = 0; i < MAX_CLIENT_CONNECTIONS; ++i) {
        io_client_watcher_t* icw = GET_CONTEXT_CLIENT(client_ctx, i);
        if (!icw) continue;

        total_clients_bytes     += ATOMIC_READ(icw->bytes);
        total_clients_processed += ATOMIC_READ(icw->processed);

        active_clients++;
        queue_lag += list_distance(LIST_TAIL(server_ctx->list), GET_LIST_ITEM(icw));
    }

    struct timeval current;
    gettimeofday(&current, NULL);

    uint64_t elapsed = _elapsed_usec(&last, &current);
    uint64_t servers_bytes     = total_servers_bytes     - last_servers_bytes;
    uint64_t clients_bytes     = total_clients_bytes     - last_clients_bytes;
    uint64_t servers_processed = total_servers_processed - last_servers_processed;
    uint64_t clients_processed = total_clients_processed - last_clients_processed;

    printf("STATS: recv %10.2f msg/s %10.2f MB/s    sent %10.2f msg/s %10.2f MB/s    qlag %10.2f    qsize %zu\n",
            servers_processed / (double) ((double) elapsed / 1000000.),
            servers_bytes     / (double) ((double) elapsed / 1000000.) / 1024 / 1024,
            clients_processed / (double) ((double) elapsed / 1000000.),
            clients_bytes     / (double) ((double) elapsed / 1000000.) / 1024 / 1024,
            (double) ((double) queue_lag) / active_clients,
            LIST_SIZE(server_ctx->list));

    memcpy(&last, &current, sizeof(current));
    last_servers_bytes = total_servers_bytes;
    last_clients_bytes = total_clients_bytes;
    last_servers_processed = total_servers_processed;
    last_clients_processed = total_clients_processed;
#endif
}

void stop_loop_cb(struct ev_loop* loop, ev_async* w, int revents) {
    _D("Async signal received in background context. Break evloop");
    ev_break (loop, EVBREAK_ALL);
}
