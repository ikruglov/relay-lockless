#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "common.h"
#include "cleanup.h"

size_t g_num_of_threads;
list_item_t** g_offsets; // array of pointers

int _is_in_offsets(list_item_t* item) {
    for (size_t i = 0; i < g_num_of_threads; ++i) {
        if (g_offsets[i] == item) {
            //_D("%p is in offsets [%p %p]", item, g_offsets[0], g_offsets[1]);
            return 1;
        }
    }

    return 0;
}

void init_cleanup(size_t num_of_threads, list_item_t* head) {
    assert(head);
    g_num_of_threads = num_of_threads;
    g_offsets = malloc(g_num_of_threads * sizeof(list_item_t*));
    if (!g_offsets) errx(EXIT_FAILURE, "Failed to calloc memory"); // TODO

    for (int i = 0; i < num_of_threads; ++i) {
        g_offsets[i] = head;
    }
}

void update_offset(size_t thread_id, list_item_t* offset) {
    // TODO assert
    g_offsets[thread_id] = offset;
}

list_item_t* shrink_list(list_item_t* head, size_t* deleted_out) {
    assert(head);

    size_t deleted = 0;
    //while (head->next && !_is_in_offsets(head)) {
    while (1) {
        if (!head->next) {
            _D("head->next is NULL %p", head);
            break;
        }

        if (_is_in_offsets(head)) {
            _D("head %p is in offsets", head);
            break;
        }

        list_item_t* tmp = head;
        head = head->next;
        list_free(tmp);
        deleted++;
    }

    if (deleted_out) *deleted_out = deleted;
    return head;
}
