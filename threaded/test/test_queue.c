#include <err.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include "list.h"
#include "common.h"

size_t time_to_run;

void* push_to_list(void* h) {
    size_t pushed = 0;
    time_t end = time(NULL) + time_to_run;
    list_item_t* head = (list_item_t*) h;

    while (time(NULL) < end) {
        head = list_new_push(head, 8);
        ++pushed;
    }

    printf("pushed: %d\n", pushed);

    size_t* ptr = malloc(sizeof(pushed));
    memcpy(ptr, &pushed, sizeof(pushed));
    pthread_exit(ptr);
}

int main(int argc, char** argv) {
    size_t tcount = argc > 1 ? atoi(argv[1]) : 10;
    time_to_run = argc > 2 ? atoi(argv[2]) : 1;

    list_item_t* head = list_init();

    pthread_t tids[tcount];
    for (int i = 0; i < tcount; ++i) {
        if (pthread_create(&tids[i], NULL, push_to_list, (void*) head))
            ERRPX("Failed to create new thread");
    }

    size_t expected_size = 1;
    for (int i = 0; i < tcount; ++i) {
        size_t* pushed = NULL;
        pthread_join(tids[i], (void**) &pushed);
        if (pushed) {
            expected_size += *pushed;
            free(pushed);
        }
    }

    size_t list_size = 1;
    while (head->next) {
        head = head->next;
        ++list_size;
    }

    _D("list size: %d expected_size: %d", list_size, expected_size);
    _D("%s", list_size == expected_size ? "MATCH" : "NO MATCH");
    return 0;
}
