#ifndef __LIST_H__
#define __LIST_H__

#include <stdint.h>

struct _list_item {
    struct _list_item* next;
    uint32_t size; // size of payload
    char data[];   // payload
};

typedef struct _list_item list_item_t;

list_item_t* list_init();
list_item_t* list_new(uint32_t size);
list_item_t* list_push(list_item_t* tail, list_item_t* item);
list_item_t* list_new_push(list_item_t* tail, uint32_t size);
void list_free(list_item_t* item);

#endif
