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
void setup_socket(socket_t* sock);

void connect_socket(socket_t* sock);
void bind_socket(socket_t* sock);

#endif
