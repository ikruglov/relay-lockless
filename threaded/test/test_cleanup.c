#include <time.h>
#include <unistd.h>
#include <pthread.h>

#include "list.h"
#include "common.h"
#include "cleanup.h"

size_t lifetime = 0;
size_t enqueued = 0;
size_t consumed = 0;
size_t deleted = 0;
int stop_enqueuer = 0;
int stop_consumer = 0;

list_item_t* last_known_tail = NULL;

pthread_t start_detached_thread(void *(*start_routine) (void*), void* arg) {
    pthread_t tid;
    pthread_attr_t attr;

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&tid, &attr, start_routine, arg);
    pthread_attr_destroy(&attr);

    return tid;
}

void* enqueuer(void* h) {
    _D("start enqueuer thread %d", (int) pthread_self());

    while (!stop_enqueuer) {
        last_known_tail = list_new_push(last_known_tail, 8);
        __sync_fetch_and_add(&enqueued, 1);
        usleep(10);
    }

    _D("end enqueuer thread %d", (int) pthread_self());
    return NULL;
}

struct consumer_arg {
    list_item_t* head;
    size_t thread_id;
};

void* consumer(void* a) {
    struct consumer_arg* arg = a;
    list_item_t* head = arg->head;
    size_t thread_id = arg->thread_id;
    _D("start consumer thread: %p %d", head, thread_id);

    while (!stop_consumer) {
        while (head->next) {
            head = head->next;
            update_offset(thread_id, head);
            __sync_fetch_and_add(&consumed, 1);
        }
    }

    _D("end consumer thread: %p", head);
    return NULL;
}

int main(int argc, char** argv) {
    size_t enq_cnt = argc > 1 ? atoi(argv[1]) : 1;
    size_t cns_cnt = argc > 2 ? atoi(argv[2]) : 1;
    lifetime = argc > 3 ? atoi(argv[3]) : 1;

    list_item_t* head = list_init();
    init_cleanup(cns_cnt, head);
    last_known_tail = head;
    _D("HEAD %p", head);

    for (int i = 0; i < enq_cnt; ++i) {
        start_detached_thread(enqueuer, NULL);
    }

    for (int i = 0; i < cns_cnt; ++i) {
        struct consumer_arg* arg = malloc(sizeof(struct consumer_arg));
        arg->head = head;
        arg->thread_id = i;

        start_detached_thread(consumer, arg);
    }

    for (int i = 0; i <= lifetime + 1; ++i) {
        size_t deleted_now = 0;
        head = shrink_list(head, &deleted_now);
        _D("shrink_list(): %d %p", deleted_now, head);
        deleted += deleted_now;

        if (i == lifetime) stop_enqueuer = 1;
        sleep(1);
    }

    stop_consumer = 1;
    sleep(1); // wait consumers to finish
    _D("HEAD %p, HEAD->next: %p, last_known_tail: %p", head, head->next, last_known_tail);

    printf("\n\n");
    _D("enqueued: %d", enqueued);
    _D("consumed: %d", consumed);
    _D("deleted: %d", deleted);

    _D("%s enqueued * consumers == consumed", (enqueued * cns_cnt == consumed) ? "MATCH" : "NO MATCH");
    _D("%s deleted == enqueued", enqueued == deleted ? "MATCH" : "NO MATCH");
}
