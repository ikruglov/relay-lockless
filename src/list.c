#include <err.h>
#include <stdlib.h>
#include <pthread.h>

#include "list.h"
#include "common.h"

list_t* list_init() {
    list_t* list = (list_t*) malloc_or_die(sizeof(list_t));
    list->head = list_new(0);
    list->tail = list->head;
    list->head->id =  0;
    list->size = 0;

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
    item->size = size;
    item->next = NULL;
    item->id = (uint64_t) -1;

    return item;
}

list_item_t* list_enqueue(list_t* list, list_item_t* item) {
    assert(list);
    assert(item);

    volatile int lock = 0;
    __sync_lock_test_and_set(&lock, 1);

    //list_item_t* tail = ATOMIC_READ(list->tail);
    //ATOMIC_WRITE(item->id, ATOMIC_READ(tail->id) + 1);
    //ATOMIC_WRITE(tail->next, item);
    //ATOMIC_WRITE(list->tail, item);
    //ATOMIC_INCREMENT(list->size);

    item->id = list->tail->id + 1;
    list->tail->next = item;
    list->tail = item;
    list->size += 1;

    __sync_lock_release(&lock);
    return item;
}

int list_dequeue(list_t* list) {
    assert(list);
    assert(list->head);

    list_item_t* head = list->head;
    if (!head->next)
        return 0;

    list->head = head->next;
    list->size -= 1;

    free(head);
    return 1;
}

size_t list_distance(list_item_t* litem, list_item_t* ritem) {
    return llabs((int64_t) litem->id - (int64_t) ritem->id);
}
