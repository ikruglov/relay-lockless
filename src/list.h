#ifndef __LIST_H__
#define __LIST_H__

#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>

struct _list_item {
    struct _list_item* next;
    uint64_t id;   // id within a list
    uint32_t size; // size of payload
#ifdef DOSTATS
    struct timeval tv;
#endif
    char data[];   // payload
};

struct _list {
    size_t size;
    struct _list_item* head;
    struct _list_item* tail;
};

typedef struct _list list_t;
typedef struct _list_item list_item_t;

// these function must be called in thread safe environment
list_t* list_init();
void list_free(list_t* list);

// these are thread safe (but only in terms of memory synchronization)
list_item_t* list_head(list_t* list);
list_item_t* list_tail(list_t* list);
list_item_t* list_new(uint32_t size);
list_item_t* list_enqueue(list_t* list, list_item_t* item);
list_item_t* list_enqueue_new(list_t* list, uint32_t size);

int list_dequeue(list_t* list);
size_t list_size(list_t* list);
size_t list_distance(list_t* list, list_item_t* item);

uint64_t list_item_id(list_item_t* item);
uint32_t list_item_size(list_item_t* item);
list_item_t* list_item_next(list_item_t* item);
// TODO list_item_data()

#endif
