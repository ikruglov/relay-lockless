#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "net.h"
#include "common.h"

size_t interval = 0;
char* data = NULL;
uint32_t data_size = 0;
size_t counter = 0;
size_t total = 0;

static uint64_t _elapsed_usec(struct timeval* start, struct timeval* end) {
    return ((end->tv_sec - start->tv_sec) * 1000000) + end->tv_usec - start->tv_usec;
}

static void* worker(void* arg) {
    printf("worker started\n");

    socket_t* sock = (socket_t*) arg;
    int use_send = sock->proto == IPPROTO_TCP;

    int fd = socket(sock->in.sin_family, sock->type, sock->proto);
    if (fd < 0) ERRPX("Failed to create socket");

    if (sock->proto == IPPROTO_TCP &&
        connect(fd, (struct sockaddr *) &sock->in, sizeof(sock->in)))
        ERRPX("Failed to connect");

    size_t offset = 0;
    size_t size = data_size + sizeof(data_size);

    while (1) {
        if (offset >= size) {
            if (ATOMIC_INCREASE(counter, 1) >= total)
                break;

            offset = 0;
        }

        ssize_t wlen = use_send
                     ?  send(fd, data + offset, size - offset, 0)
                     :  sendto(fd, data + offset, size - offset, 0, (struct sockaddr*) &sock->in, sizeof(sock->in));

        if (wlen <= 0) {
            printf("Failed to sent data %zu %s\n", wlen, strerror(errno));
            break;
        }

        offset += wlen;
        if (interval > 0) {
            usleep(interval);
        }
    }

    printf("worker exited\n");
    return NULL;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: tclient udp@localhost:2008 [threads] [cnt] [size] [interval]\n");
        return EXIT_FAILURE;
    }

    socket_t* sock = socketize(argv[1]);

    size_t threads = argc > 2 ? atoi(argv[2]) : 1;
    total          = argc > 3 ? atoi(argv[3]) : (size_t) -1;
    data_size      = argc > 4 ? atoi(argv[4]) : 32 * 1024;
    interval       = argc > 5 ? atoi(argv[5]) : 0;

    data = malloc(data_size + sizeof(data_size));
    memcpy(data, &data_size, sizeof(data_size));

    printf("starting tclient to %s in %zu threads, cnt: %zu, size: %zu\n",
            sock->to_string, threads, total, (size_t) data_size);

    for (int i = 0; i < threads; ++i) {
        pthread_t tid;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        pthread_create(&tid, &attr, worker, sock);
        pthread_attr_destroy(&attr);
    }

    size_t last_counter = 0;
    struct timeval start;
    gettimeofday(&start, NULL);
    time_t last_reported = time(0);

    while (1) {
        time_t now = time(0);
        size_t current_counter = ATOMIC_READ(counter);

        if (last_reported != now) {
            printf("%d rate %zu msg/s, total sent: %zu\n",
                   (int) now, current_counter - last_counter, current_counter);

            last_counter = current_counter;
            last_reported = now;
        }

        if (current_counter >= total) break;
        usleep(100);
    }

    struct timeval end;
    gettimeofday(&end, NULL);
    uint64_t elapsed = _elapsed_usec(&start, &end);

    printf("\n=============================\n");
    printf("sent %zu packets of size %zu within %.2f seconds\n",
           ATOMIC_READ(counter), (size_t) data_size, ((double) elapsed / 1000000.));

    return EXIT_SUCCESS;
}
