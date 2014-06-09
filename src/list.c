#include <err.h>
#include <stdlib.h>

#include "list.h"
#include "common.h"

list_t* list_init() {
    list_t* list = (list_t*) malloc_or_die(sizeof(list_t));
    list->head = list_new(0);
    list->tail = list->head;
    list->head->id = 0;
    list->size = 0;
    return list;
}

list_item_t* list_new(uint32_t size) {
    list_item_t* item = (list_item_t*) malloc_or_die(sizeof(list_item_t) + size);
    item->size = size;
    item->next = NULL;
    item->id = (uint64_t) -1;
    return item;
}

list_item_t* list_enqueue(list_t* list, list_item_t* item) {
    assert(item);
    assert(list);
    assert(list->tail);
    assert(list->tail->next == NULL);

    item->id = list->tail->id + 1;
    list->tail->next = item;
    list->tail = item;
    list->size += 1;
    return list->tail;
}

list_item_t* list_enqueue_new(list_t* list, uint32_t size) {
    return list_enqueue(list, list_new(size));
}

int list_dequeue(list_t* list) {
    assert(list->head);
    assert(list->tail);

    list_item_t* head = list->head;
    if (head->next == NULL)
        return 0;

    list->head = head->next;
    list->size -= 1;
    free(head);
    return 1;
}

size_t list_size(list_t* list) {
    assert(list);
    assert(list->head);
    assert(list->tail);

#ifdef DEBUG
    size_t size = 0;
    list_item_t* head = list->head;

    while (head->next) {
        head = head->next;
        ++size;
    }

    assert(size == list->size);
    assert(head == list->tail);
#endif

    return list->size;
}
