#ifndef __LIST_H__
#define __LIST_H__

#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>

struct _list_item {
    struct _list_item* next;
    uint64_t id;   // id within a list
    uint32_t size; // size of payload
    char data[];   // payload
};

struct _list {
    struct _list_item* head;
    struct _list_item* tail;
    size_t size;
};

typedef struct _list list_t;
typedef struct _list_item list_item_t;

list_t* list_init();
void list_free(list_t* list);

list_item_t* list_new(uint32_t size);
list_item_t* list_enqueue(list_t* list, list_item_t* item);
int list_dequeue(list_t* list);
size_t list_distance(list_item_t* litem, list_item_t* ritem);

#endif
