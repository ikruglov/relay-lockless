#ifndef __LIST_H__
#define __LIST_H__

#include <time.h>
#include <stdio.h>
#include <stdint.h>

struct _list_item {
    struct _list_item* next;
    uint64_t id;   // id within a list
    uint32_t size; // size of payload
    time_t time;   // epoch when item was enqueued
    char data[];   // payload
};

struct _list {
    size_t size;
    struct _list_item* head;
    struct _list_item* tail;
};

#define LIST_HEAD(__l) ATOMIC_READ((__l)->head)
#define LIST_TAIL(__l) ATOMIC_READ((__l)->tail)
#define LIST_SIZE(__l) ATOMIC_READ((__l)->size)

#define LIST_ITEM_ID(__i) ATOMIC_READ((__i)->id)
#define LIST_ITEM_SIZE(__i) ATOMIC_READ((__i)->size)
#define LIST_ITEM_NEXT(__i) ATOMIC_READ((__i)->next)
//#define LIST_ITEM_DATA

typedef struct _list list_t;
typedef struct _list_item list_item_t;

// these function must be called in thread safe environment
list_t* list_init();
void list_free(list_t* list);

// these are thread safe (but only in terms of memory synchronization)
list_item_t* list_new(uint32_t size);
list_item_t* list_enqueue(list_t* list, list_item_t* item);

int list_dequeue(list_t* list);
size_t list_distance(list_item_t* litem, list_item_t* ritem);

#endif
