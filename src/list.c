#include <err.h>
#include <stdlib.h>

#include "list.h"
#include "common.h"

list_t* list_init() {
    list_t* list = (list_t*) malloc_or_die(sizeof(list_t));
    list->head = list_new(0);
    list->tail = list->head;
    return list;
}

list_item_t* list_new(uint32_t size) {
    list_item_t* item = (list_item_t*) malloc_or_die(sizeof(list_item_t) + size);
    item->size = size;
    item->next = NULL;
    return item;
}

list_item_t* list_enqueue(list_t* list, list_item_t* item) {
    assert(item);
    assert(list);
    assert(list->tail);
    assert(list->tail->next == NULL);

    list->tail->next = item;
    list->tail = item;
    return list->tail;
}

list_item_t* list_enqueue_new(list_t* list, uint32_t size) {
    return list_enqueue(list, list_new(size));
}

void list_dequeue(list_t* list) {
    assert(list->head);
    assert(list->tail);

    list_item_t* head = list->head;
    if (head->next == NULL) return;

    list->head = head->next;
    free(head);
}

size_t list_size(list_t* list) {
    assert(list->head);
    assert(list->tail);

    size_t size = 0;
    list_item_t* head = list->head;

    while (head->next) {
        head = head->next;
        ++size;
    }

    assert(head == list->tail);
    return size;
}
