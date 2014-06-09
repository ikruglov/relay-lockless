CC=gcc
CFLAGS=-std=c99 -O0 -g3 -Wall -pthread
INCLUDE=-I src

test_queue:
	@mkdir -p bin
	$(CC) $(CFLAGS) $(INCLUDE) src/list.c test/test_queue.c -o bin/test_queue

test_cleanup:
	@mkdir -p bin
	$(CC) $(CFLAGS) $(INCLUDE) -D_BSD_SOURCE src/list.c src/cleanup.c test/test_cleanup.c -o bin/test_cleanup

test: test_queue test_cleanup

clean:
	rm -rf bin
