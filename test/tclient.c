#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "net.h"
#include "common.h"

char* data = NULL;
size_t data_size = 0;
size_t counter = 0;
size_t total = 0;

static void* worker(void* arg) {
    printf("worker started\n");

    socket_t* sock = (socket_t*) arg;
    int fd = sock->socket;
    int use_send = sock->proto == IPPROTO_TCP;

    while (1) {
        ssize_t wlen = use_send
                     ?  send(fd, data, data_size, 0)
                     :  sendto(fd, data, data_size, 0, (struct sockaddr*) &sock->in, sizeof(sock->in));

        if (wlen < 0) break;

        ATOMIC_INCREMENT(counter);
        if (ATOMIC_READ(counter) >= ATOMIC_READ(total))
            break;
    }

    printf("worker exited\n");
    return NULL;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: tclient udp@localhost:2008 [threads] [cnt] [size]\n");
        return EXIT_FAILURE;
    }

    socket_t* sock = socketize(argv[1]);

    size_t threads = argc > 2 ? atoi(argv[2]) : 1;
    total          = argc > 3 ? atoi(argv[3]) : (size_t) -1;
    data_size      = argc > 4 ? atoi(argv[4]) : 32 * 1024;

    data = malloc(data_size);

    printf("starting client to %s in %zu threads, cnt: %zu, size: %zu\n",
            sock->to_string, threads, total, data_size);

    int fd = socket(sock->in.sin_family, sock->type, sock->proto);
    if (fd < 0) ERRPX("Failed to create socket");
    sock->socket = fd;

    if (sock->proto == IPPROTO_TCP &&
        connect(fd, (struct sockaddr *) &sock->in, sizeof(sock->in)))
        ERRPX("Failed to connect");

    for (int i = 0; i < threads; ++i) {
        pthread_t tid;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        pthread_create(&tid, &attr, worker, sock);
        pthread_attr_destroy(&attr);
    }

    size_t last_counter = 0;
    while (1) {
        size_t current_counter = ATOMIC_READ(counter);
        size_t diff = current_counter - last_counter;
        last_counter = current_counter;

        printf("%d rate %zu pps, total sent: %zu\n", (int) time(0), diff, current_counter);

        if (current_counter >= ATOMIC_READ(total))
            break;

        sleep(1);
    }

    return EXIT_SUCCESS;
}
