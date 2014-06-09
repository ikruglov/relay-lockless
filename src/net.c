#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>

#include "net.h"
#include "common.h"

static int set_nonblocking(int sock) {
    long fl = fcntl(sock, F_GETFL);
    return fl == -1 ? fl : fcntl(sock, F_SETFL, fl | O_NONBLOCK);
}

socket_t* socketize(const char* a) {
    assert(a);
    char* arg = strdup(a);

    socket_t* sock = calloc(1, sizeof(socket_t));
    if (!sock) ERRPX("Failed to calloc");

    if (strncmp(arg, "tcp@", 4) == 0) {
        sock->type = SOCK_STREAM;
        sock->proto = IPPROTO_TCP;
    } else if (strncmp(arg, "udp@", 4) == 0) {
        sock->type = SOCK_DGRAM;
        sock->proto = IPPROTO_UDP;
    } else {
        ERRPX("Unknown format for conf-string. ex: udp@localhost:6379");
    }

    char* colon = strrchr(arg, ':');
    if (!colon) ERRPX("Unknown format for conf-string. ex: udp@localhost:6379");
    sock->port = atoi(colon + 1);

    *colon = '\0';
    char* hostname = arg + 4;

    struct hostent* host = gethostbyname2(hostname, AF_INET);
    if (!host) ERRPX("Failed to parse/resolve %s", arg);

    sock->in.sin_family = AF_INET;
    sock->in.sin_port = htons(sock->port);
    memcpy(&sock->in.sin_addr, host->h_addr, host->h_length);

    snprintf(sock->to_string, PATH_MAX,
            "%s@%s:%d", (sock->proto == IPPROTO_TCP ? "tcp" : "udp"),
            inet_ntoa(sock->in.sin_addr), ntohs(sock->in.sin_port));

    _D("socketize: %s", sock->to_string);

    sock->socket = -1;

    free(arg);
    return sock;
}

void setup_socket(socket_t* sock) {
    assert(sock);

    int fd = socket(sock->in.sin_family, sock->type, sock->proto);
    if (fd < 0) ERRPX("Failed to create socket %s", sock->to_string);

    if (set_nonblocking(fd)) {
        close(fd);
        ERRPX("Failed to set non blocking mode for socket [fd:%d] %s", fd, sock->to_string);
    }

    sock->socket = fd;
}

void connect_socket(socket_t* sock) {
    assert(sock);

    int fd = socket(sock->in.sin_family, sock->type, sock->proto);
    if (fd < 0) ERRPX("Failed to create socket %s", sock->to_string);

    if (   sock->proto == IPPROTO_TCP
        && connect(fd, (struct sockaddr *) &sock->in, sizeof(sock->in))
    ) {
        close(fd);
        ERRPX("Failed to connect to socket %s", sock->to_string);
    }

    if (fcntl(fd, F_SETFL, O_NONBLOCK)) {
        close(fd);
        ERRPX("Failed to set non-blocking mode for socket %s", sock->to_string);
    }

    sock->socket = fd;
}

void bind_socket(socket_t* sock) {
    assert(sock);

    int fd = socket(sock->in.sin_family, sock->type, sock->proto);
    if (fd < 0) ERRPX("Failed to create socket %s", sock->to_string);

    if (bind(fd, (struct sockaddr *) &sock->in, sizeof(sock->in))) {
        close(fd); 
        ERRPX("Failed to bind socket %s", sock->to_string);
    }

    if (sock->proto == IPPROTO_TCP && listen(fd, SOMAXCONN)) {
        close(fd);
        ERRPX("Failed to listen on socket %s", sock->to_string);
    }

    if (fcntl(fd, F_SETFL, O_NONBLOCK)) {
        close(fd);
        ERRPX("Failed to set non-blocking mode for socket %s", sock->to_string);
    }

    sock->socket = fd;
}
