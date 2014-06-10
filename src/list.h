#ifndef __LIST_H__
#define __LIST_H__

#include <stdio.h>
#include <stdint.h>

#define ATOMIC_READ(__p)                __sync_fetch_and_add(&__p, 0)
#define ATOMIC_INCREMENT(__i, __cnt)    __sync_fetch_and_add(&__i, __cnt);
#define ATOMIC_DECREMENT(__i, __cnt)    __sync_fetch_and_sub(&__i, __cnt);
#define ATOMIC_CMPXCHG(__p, __v1, __v2) __sync_bool_compare_and_swap(&__p, __v1, __v2)

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

typedef struct _list list_t;
typedef struct _list_item list_item_t;

list_t* list_init();
void list_free(list_t* list);

list_item_t* list_head(list_t* list);
list_item_t* list_tail(list_t* list);
list_item_t* list_new(uint32_t size);
list_item_t* list_enqueue(list_t* list, list_item_t* item);
list_item_t* list_enqueue_new(list_t* list, uint32_t size);

int list_dequeue(list_t* list);
size_t list_size(list_t* list);
size_t list_distance(list_t* list, list_item_t* item);

uint64_t list_item_id(list_item_t* item);
list_item_t* list_item_next(list_item_t* item);

#endif
