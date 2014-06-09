#ifndef __LIST_H__
#define __LIST_H__

#include <stdio.h>
#include <stdint.h>

struct _list_item {
    struct _list_item* next;
    uint32_t size; // size of payload
    char data[];   // payload
};

struct _list {
    struct _list_item* head;
    struct _list_item* tail;
};

typedef struct _list list_t;
typedef struct _list_item list_item_t;

list_t* list_init();
list_item_t* list_new(uint32_t size);
list_item_t* list_enqueue(list_t* list, list_item_t* item);
list_item_t* list_enqueue_new(list_t* list, uint32_t size);
void list_dequeue(list_t* list);
size_t list_size(list_t* list); // O(n)

#endif
