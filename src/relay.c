#include <signal.h>

#include "ev.h"
#include "net.h"
#include "list.h"
#include "ev_cb.h"
#include "common.h"

static void sig_handler(int signum) {
    _D("IGNORE: unexpected signal %d", signum);
}

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("Usage: ./bin/relay udp@localhost:10000 tcp@localhost:10001 ...\n");
        return EXIT_FAILURE;
    }

    // setup signal hendlers
    signal(SIGPIPE, sig_handler);
    signal(SIGCHLD, sig_handler);

    // init server context
    context_t* ctx = init_context();

    // init libev
    struct ev_loop* loop = ev_default_loop(0);
    ev_set_userdata(loop, ctx);

    // init udp socket
    //socket_t* listened_sock = socketize("udp@localhost:2008");
    socket_t* listened_sock = socketize(argv[1]);
    if (setup_socket(listened_sock, 1)) {
        ERRX("Failed to setup socket");
    }

    if (listened_sock->proto == IPPROTO_TCP) {
        new_io_server_watcher(loop, tcp_accept_cb, listened_sock);
    } else {
        new_io_server_watcher(loop, udp_server_cb, listened_sock);
    }

    // init clients
    for (int i = 2; i < argc; ++i) {
        socket_t* client_sock = socketize(argv[i]);
        setup_socket(client_sock, 0);
        io_client_watcher_t* tcp_client = new_io_client_watcher(loop, tcp_client_cb, client_sock);
        try_connect(tcp_client);
    }

    ev_timer reconnect_timer;
    ev_timer_init(&reconnect_timer, reconnect_clients_cb, 0, 1);
    ev_timer_start(loop, &reconnect_timer);

    ev_timer cleanup_list;
    ev_timer_init(&cleanup_list, cleanup_list_cb, 1, 1);
    ev_timer_start(loop, &cleanup_list);

    ev_run(loop, 0);

    free_context(ctx);
    return EXIT_SUCCESS;
}
