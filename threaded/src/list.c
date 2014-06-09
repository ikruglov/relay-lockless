#include <err.h>
#include <stdlib.h>

#include "list.h"
#include "common.h"

list_item_t* _malloc_or_die(size_t size) {
    void* ptr = malloc(size);
    if (!ptr) ERRPX("Failed to allocate %zu", size);
    return (list_item_t*) ptr;
}

list_item_t* list_init() {
    list_item_t* item = _malloc_or_die(sizeof(list_item_t));
    item->next = NULL;
    item->size = 0;
    return item;
}

list_item_t* list_new(uint32_t size) {
    list_item_t* item = _malloc_or_die(sizeof(list_item_t) + size);
    item->size = size;
    item->next = NULL;
    return item;
}

list_item_t* list_push(list_item_t* tail, list_item_t* item) {
    for (int i = 0; i < 10000; ++i) { // while(1) ???
        if (__sync_bool_compare_and_swap(&tail->next, NULL, item))
            return item;

        while (tail->next) tail = tail->next;
    }

    _D("BAH!!!!!!");
    return NULL;
}

list_item_t* list_new_push(list_item_t* tail, uint32_t size) {
    return list_push(tail, list_new(size));
}

void list_free(list_item_t* item) {
    if (item) free(item);
}
