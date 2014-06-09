#include "common.h"

void* malloc_or_die(size_t size) {
    void* ptr = malloc(size);
    if (!ptr) ERRPX("Failed to malloc %zu bytes", size);
    return ptr;
}

void* calloc_or_die(size_t nmemb, size_t size) {
    void* ptr = calloc(nmemb, size);
    if (!ptr) ERRPX("Failed to calloc %zu bytes", nmemb * size);
    return ptr;
}
