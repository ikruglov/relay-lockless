#include "list.h"
#include "common.h"

int main(int argc, char** argv) {
    list_t* list = list_init();
    assert(list_size(list) == 0);

    list_enqueue_new(list, 8);
    list_enqueue_new(list, 8);
    list_enqueue_new(list, 8);
    assert(list_size(list) == 3);

    assert(list->head->id == 0);
    assert(list->head->next->id == 1);
    assert(list->head->next->next->id == 2);
    assert(list->head->next->next->next->id == 3);
    assert(list->tail->id == 3);

    list_dequeue(list);
    list_dequeue(list);
    list_dequeue(list);
    assert(list_size(list) == 0);

    list_dequeue(list);
    assert(list_size(list) == 0);

    list_enqueue_new(list, 8);
    assert(list_size(list) == 1);
    assert(list->tail->id == 4);
}
