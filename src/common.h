#ifndef __COMMON_H__
#define __COMMON_H__

#include <err.h>
#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>

#define FORMAT(fmt, arg...) \
        fmt " [%s():%s:%d @ %u]\n", ##arg, __func__, __FILE__, __LINE__, (unsigned int) time(NULL)
#define ERRX(fmt, arg...) \
        errx(EXIT_FAILURE, FORMAT(fmt, ##arg));
#define ERRPX(fmt, arg...) \
        ERRX(fmt ": %s", ##arg, errno ? strerror(errno) : "undefined error")
#define _E(fmt,arg...) \
        printf(FORMAT(fmt ": %s", ##arg, errno ? strerror(errno) : "undefined error"))

#ifdef DEBUG
#define _D(fmt,arg...) \
        printf(FORMAT(fmt, ##arg))
#else
#define _D(fmt,arg...)
#endif

inline static
void* malloc_or_die(size_t size) {
    void* ptr = malloc(size);
    if (!ptr) ERRPX("Failed to malloc %zu bytes", size);
    return ptr;
}

inline static
void* calloc_or_die(size_t nmemb, size_t size) {
    void* ptr = calloc(nmemb, size);
    if (!ptr) ERRPX("Failed to calloc %zu bytes", nmemb * size);
    return ptr;
}

#endif
