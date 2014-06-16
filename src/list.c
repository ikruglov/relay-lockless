#include <err.h>
#include <stdlib.h>

#include "list.h"
#include "common.h"

// thread non-safe functions
list_t* list_init() {
    list_t* list = (list_t*) malloc_or_die(sizeof(list_t));
    ATOMIC_WRITE(list->head, list_new(0));
    ATOMIC_WRITE(list->tail, list->head);
    ATOMIC_WRITE(list->head->id, 0);
    ATOMIC_WRITE(list->size, 0);
    return list;
}

void list_free(list_t* list) {
    if (!list) return;
    while (list_dequeue(list));

    assert(list->head == list->tail);
    free(list->head);
    free(list);
}

// thread safe functions
list_item_t* list_new(uint32_t size) {
    list_item_t* item = (list_item_t*) malloc_or_die(sizeof(list_item_t) + size);
    ATOMIC_WRITE(item->size, size);
    ATOMIC_WRITE(item->next, NULL);
    ATOMIC_WRITE(item->id, (uint64_t) -1);
    return item;
}

list_item_t* list_enqueue(list_t* list, list_item_t* item) {
    assert(list);
    assert(item);

    list_item_t* tail = LIST_TAIL(list);
    ATOMIC_WRITE(item->id, LIST_ITEM_ID(tail) + 1);
    ATOMIC_WRITE(tail->next, item);
    ATOMIC_WRITE(list->tail, item);
    ATOMIC_INCREMENT(list->size);

    return item;
}

int list_dequeue(list_t* list) {
    assert(list);

    list_item_t* head = LIST_HEAD(list);
    list_item_t* next = LIST_ITEM_NEXT(head);
    if (!next) return 0;

    ATOMIC_WRITE(list->head, next);
    ATOMIC_DECREMENT(list->size);

    free(head);
    return 1;
}

size_t list_distance(list_item_t* litem, list_item_t* ritem) {
    return llabs((int64_t) ATOMIC_READ(litem->id) - (int64_t) ATOMIC_READ(ritem->id));
}
