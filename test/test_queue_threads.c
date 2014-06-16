#include <assert.h>
#include <pthread.h>

#include "list.h"
#include "common.h"

void* reader(void* arg) {
    list_t* list = (list_t*) arg;
    printf("reader started\n");

    while (1) {
        list_item_t* prev = list->head;
        for (list_item_t* next = prev->next; next; prev = next, next = next->next) {
            assert(next->id == prev->id + 1);
        }
    }
}

int main(int argc, char** argv) {
    list_t* list = list_init();
    size_t sec = argc > 1 ? atoi(argv[1]) : 1;
    size_t threads = argc > 2 ? atoi(argv[2]) : 1;

    for (size_t i = 0; i < threads; ++i) {
        pthread_t tid;
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        pthread_create(&tid, &attr, reader, list);
        pthread_attr_destroy(&attr);
    }

    size_t cnt = 0;
    clock_t start = clock();
    clock_t end = clock() + sec * CLOCKS_PER_SEC;
    while (clock() < end) {
        list_enqueue(list, list_new(0));
        ++cnt;
    }

    printf("enqueued %zu with in %.2f seconds\n", cnt, (double) (clock() - start) / CLOCKS_PER_SEC);
}
