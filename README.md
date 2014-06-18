relay-lockless
==============

UDP to TCP socket -> socket relay.

```
UDP -----\           /----- TCP
UDP -------> queue ->------ TCP
UDP -----/           \----- TCP
```

Special effort is done to not have any locks (mostly for non-practical reasons)

Idea of relay is borrowed from https://github.com/demerphq/relay

## Install and usage
* git clone https://github.com/ikruglov/relay-lockless
* cd relay-lockless
* make
* ./bin/relay udp@localhost:2008 tcp@localhost:2009 tcp@localhost:2010 ...

## Application design

Application runs three thread:
* server thread - this thread receives incoming data
* client thread - this thread sends data to final destinations 
* background thread - thread for periodic tasks

Server and client threads run libev-powered event loop and
interact between each other via queue. Hence both threads are
able to server many connections. Connections, which are served
by server thread (i.e. which send dato to relay), are called
servers, the once which are server by client thread (i.e. which
send data to final destinations) are called clients.

The queue is a single-linked list. Its usage workflow has some
remarkable features:

* an item has unique id which monotonically increases
* only server thread enqueue items and always to the tail
* each client holds a pointer to an item in the queue (offset)
```
    ------------------------------------------>
             ^  ^           ^             ^
       client1  client2   client3      client4
```

* client move forward inside the queue, never backwards
* periodical cleanup of the queue is done by background thread.
  See "Backgroung thread workflow"


### Server thread workflow

1. Receive a UDP packet
2. Create new queue item and copy the packet to associated to the item buffer
3. Push item to tail of the queue and assign new id = lastid + 1

### Client thread workflow

1. Check if there is a new item after the offset
2. Advance to it and update offset
3. Send item's content

### Background thread workflow

The aim of background thread is to periodicaly shrink the queue.
It's done by applying following algorithm:

```
    int min_id = MAX_INT;
    for each client
        if (client->list_item->id < min_id)
            min_id = client->list_item->id;

    while (list->size && list->head->id < min_id)
        deqeueue_list(list)
```

Comparasion of IDs garantees that current item can be sefely dequeued
because nobody holds a pointer to it or to any items before it.

### Data structures shared between threads

Access to following data structures should be synchronized:

```
# list.h
struct _list_item {
    struct _list_item* next;
    uint64_t id;   // id within a list
    uint32_t size; // size of payload
    char data[];   // payload
};

struct _list {
    size_t size;
    struct _list_item* head;
    struct _list_item* tail;
};

# server_ctx.h
struct _server_context {
    ...
    uint64_t bytes, processed;
    ...
}

# client_ctx.h
struct _client_context {
    ...
    uint64_t bytes, processed;
    size_t active_clients, total_clients;
    io_client_watcher_t* clients[MAX_CLIENT_CONNECTIONS];
    ...
};
```
