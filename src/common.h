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

#define STMT_START do
#define STMT_END while (0)

#define ERRX(fmt, arg...) \
        errx(EXIT_FAILURE, fmt, ##arg);
#define ERRPX(fmt, arg...) \
        ERRX(fmt " { %s }", ##arg, errno ? strerror(errno) : "undefined error");
#define FORMAT(fmt,arg...) \
        fmt " [%s():%s:%d @ %u]\n", ##arg, __func__, __FILE__, __LINE__, (unsigned int) time(NULL)

#define _D(fmt,arg...) printf(FORMAT(fmt, ##arg))
#define _E(fmt,arg...) fprintf(stderr,FORMAT(fmt,##arg))

#endif
