#include "list.h"
#include "common.h"

int main(int argc, char** argv) {
    list_t* list = list_init();
    assert(list_size(list) == 0);

    list_enqueue_new(list, 8);
    list_enqueue_new(list, 8);
    list_enqueue_new(list, 8);
    assert(list_size(list) == 3);

    list_dequeue(list);
    list_dequeue(list);
    list_dequeue(list);
    assert(list_size(list) == 0);

    list_dequeue(list);
    assert(list_size(list) == 0);

    list_enqueue_new(list, 8);
    assert(list_size(list) == 1);
}
