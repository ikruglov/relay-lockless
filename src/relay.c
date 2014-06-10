#include <signal.h>
#include <pthread.h>

#include "ev.h"
#include "net.h"
#include "list.h"
#include "common.h"
#include "client_ctx.h"
#include "server_ctx.h"
#include "background_ctx.h"

static void sig_handler(int signum);
static void* start_event_loop(void* arg);
static pthread_t start_thread(void *(*start_routine) (void*), void* arg, int detached);

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("Usage: ./bin/relay udp@localhost:10000 tcp@localhost:10001 ...\n");
        return EXIT_FAILURE;
    }

    // setup signal hendlers
    signal(SIGPIPE, sig_handler);
    signal(SIGCHLD, sig_handler);

    // init contexts
    client_ctx_t* client_ctx = init_client_context();
    server_ctx_t* server_ctx = init_server_context(client_ctx);
    bg_ctx_t* bg_ctx = init_bg_context(server_ctx, client_ctx);

    // init server
    socket_t* listened_sock = socketize(argv[1]);
    if (setup_socket(listened_sock, 1)) {
        ERRX("Failed to setup socket");
    }

    if (listened_sock->proto == IPPROTO_TCP) {
        init_io_server_watcher(server_ctx, tcp_accept_cb, listened_sock);
    } else {
        init_io_server_watcher(server_ctx, udp_server_cb, listened_sock);
    }

    // init clients
    for (int i = 2; i < argc; ++i) {
        socket_t* client_sock = socketize(argv[i]);
        setup_socket(client_sock, 0);
        io_client_watcher_t* tcp_client =
            init_io_client_watcher(client_ctx, tcp_client_cb, client_sock);

        try_connect(tcp_client);
    }

    start_thread(start_event_loop, bg_ctx->loop, 1);
    start_thread(start_event_loop, client_ctx->loop, 1);

    pthread_t server_tid = start_thread(start_event_loop, server_ctx->loop, 0);
    pthread_join(server_tid, NULL);
    sleep(10);

    free_bg_context(bg_ctx);
    free_client_context(client_ctx);
    free_server_context(server_ctx);
    return EXIT_SUCCESS;
}

void* start_event_loop(void* arg) {
    ev_run((struct ev_loop*) arg, 0);
    return NULL;
}

pthread_t start_thread(void *(*start_routine) (void*), void* arg, int detached) {
    pthread_t tid;

    if (detached) {
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        pthread_create(&tid, &attr, start_routine, arg);
        pthread_attr_destroy(&attr);
    } else {
        pthread_create(&tid, NULL, start_routine, arg);
    }

    return tid;
}

void sig_handler(int signum) {
    _D("IGNORE: unexpected signal %d", signum);
}
