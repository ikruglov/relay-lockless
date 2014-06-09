#include <stdio.h>
#include <unistd.h>
#include <pthread.h>

#include "list.h"
#include "cleanup.h"

char** CONFIG = NULL;
list_item_t* g_HEAD = NULL;

pthread_t start_detached_thread(void *(*start_routine) (void*), void* arg) {
    pthread_t tid;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&tid, &attr, start_routine, arg);
    pthread_attr_destroy(&attr);

    return tid;
}

void* enqueue_items(void* h) {
    size_t enqueued = 0;
    list_item_t* head = h;
    time_t end = time(NULL) + 5;

    while (time(NULL) < end) {
        head = list_new_push(head, 8);
        update_offset(0, head);
        ++enqueued;
    }

    printf("enqueued: %d\n", enqueued);
    return NULL;
}

int main(int argc, char** argv) {
    g_HEAD = list_init();

    init_cleanup(1, g_HEAD);
    start_detached_thread(periodically_shrink_list, &g_HEAD);

    start_detached_thread(enqueue_items, g_HEAD);
    start_detached_thread(enqueue_items, g_HEAD);

    sleep(5);
    printf("first sleep done\n");
    stop_cleanup();
    sleep(1);
}
