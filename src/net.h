#ifndef __NET_H__
#define __NET_H__

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/limits.h>

struct _socket {
    int type;
    int port;
    int proto;
    int socket;
    struct sockaddr_in in;
    char to_string[PATH_MAX];
};

typedef struct _socket socket_t;

socket_t* socketize(const char* arg);
socket_t* socketize_sockaddr(const struct sockaddr_in* sockaddr);
int setup_socket(socket_t* sock, int server_mode); //TODO replace to flags

#endif
