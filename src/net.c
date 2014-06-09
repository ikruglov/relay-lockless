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
    socket_t* sock = calloc_or_die(1, sizeof(socket_t));

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

socket_t* socketize_sockaddr(const struct sockaddr_in* sockaddr) {
    assert(sockaddr);

    socket_t* sock = calloc_or_die(1, sizeof(socket_t));

    sock->socket = -1;
    sock->proto = IPPROTO_TCP;
    sock->type = SOCK_STREAM;
    sock->port = ntohs(sockaddr->sin_port);

    memcpy(&sock->in, sockaddr, sizeof(sockaddr));

    snprintf(sock->to_string, PATH_MAX,
            "%s@%s:%d", (sock->proto == IPPROTO_TCP ? "tcp" : "udp"),
            inet_ntoa(sock->in.sin_addr), ntohs(sock->in.sin_port));

    _D("socketize: %s", sock->to_string);
    
    return sock;
}

int setup_socket(socket_t* sock, int server_mode) {
    assert(sock);

    int fd = socket(sock->in.sin_family, sock->type, sock->proto);
    if (fd < 0) {
        _E("Failed to create socket [%s]", sock->to_string);
        return -1;
    }

    if (server_mode) {
        int yes = 1;
        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes))) {
            _E("Failed to setsockopt SO_REUSEADDR [%s]", sock->to_string);
            close(fd); 
            return -1;
        }

        if (bind(fd, (struct sockaddr *) &sock->in, sizeof(sock->in))) {
            _E("Failed to bind socket [%s]", sock->to_string);
            close(fd); 
            return -1;
        }

        if (sock->proto == IPPROTO_TCP && listen(fd, SOMAXCONN)) {
            _E("Failed to listen socket [%s]", sock->to_string);
            close(fd);
            return -1;
        }
    }

    if (set_nonblocking(fd)) {
        _E("Failed to set non blocking mode [%s]", sock->to_string);
        close(fd);
        return -1;
    }

    sock->socket = fd;
    return 0;
}
