#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "net.h"
#include "common.h"

uint64_t bytes;
uint64_t packets;

static void* tcp_worker(void* arg) {
    socket_t* sock = (socket_t*) arg;
    printf("worker started %s\n", sock->to_string);

    char buf[64 * 1024]; 
    while (1) {
        ssize_t rlen = recv(sock->socket, buf, sizeof(buf), MSG_WAITALL);
        if (rlen > 0) {
            ATOMIC_INCREMENT(packets);
            ATOMIC_INCREASE(bytes, rlen);
        } else if (rlen == 0) {
            printf("shutdown %s\n", sock->to_string);
            break;
        } else {
            printf("recv return errror %s: %s\n", sock->to_string, strerror(errno));
            break;
        }
    }

    printf("worker exited %s\n", sock->to_string);
    free(sock);
    return NULL;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("Usage: tserver tcp@localhost:2008\n");
        return EXIT_FAILURE;
    }

    socket_t* sock = socketize(argv[1]);
    if (sock->proto != IPPROTO_TCP) ERRX("Support only TCP");
    if (setup_socket(sock, 1)) ERRX("Failed to setup socket");

    printf("starting tserver %s\n", sock->to_string);

    while (1) {
        struct sockaddr_in addr;
        socklen_t addrlen = sizeof(addr);
        int client = accept(sock->socket, (struct sockaddr*) &addr, &addrlen);

        if (client >= 0) {
            socket_t* csock = socketize_sockaddr(&addr);
            csock->socket = client;

            pthread_t tid;
            pthread_attr_t attr;
            pthread_attr_init(&attr);
            pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
            pthread_create(&tid, &attr, tcp_worker, csock);
            pthread_attr_destroy(&attr);
        } else if (errno != EAGAIN) {
            ERRPX("Failed to accept connection");
        }

        usleep(1000);
    }

    return EXIT_SUCCESS;
}
