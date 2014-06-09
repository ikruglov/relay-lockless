#include <ev.h>
#include <signal.h>

#include "net.h"
#include "list.h"
#include "ev_cb.h"
#include "common.h"

static void sig_handler(int signum) {
    _D("IGNORE: unexpected signal %d", signum);
}

int main(int argc, char** argv) {
    // setup signal hendlers
    signal(SIGPIPE, sig_handler);
    signal(SIGCHLD, sig_handler);

    // init server context
    context_t* ctx = init_context();

    // init libev
    struct ev_loop* loop = ev_default_loop(0);
    ev_set_userdata(loop, ctx);

    // init udp socket
    socket_t* listened_sock = socketize("udp@localhost:2008");
    bind_socket(listened_sock);
    new_io_server_watcher(loop, udp_server_cb, listened_sock);

    // init client
    socket_t* client_sock = socketize("tcp@localhost:2009");
    setup_socket(client_sock);
    io_client_watcher_t* tcp_client = new_io_client_watcher(loop, tcp_client_cb, client_sock);
    //try_to_connect(tcp_client);

    ev_timer reconnect_timer;
    ev_timer_init(&reconnect_timer, reconnect_clients_cb, 0, 1);
    ev_timer_start(loop, &reconnect_timer);

    ev_timer wakeup_clients_timer;
    ev_timer_init(&wakeup_clients_timer, wakeup_clients, 0, 0.1);
    ev_timer_start(loop, &wakeup_clients_timer);

    ev_run(loop, 0);
    return 1;
}
