CC=gcc
CFLAGS=-std=gnu99 -O3 -g -Wall -DDEBUG -DEV_STANDALONE=1 -fno-strict-aliasing
INCLUDE=-I src -I libev

all: relay

mkdir:
	@mkdir -p bin

relay: mkdir
	$(CC) -c -O3 -g -DEV_STANDALONE=1 libev/ev.c -o ev.o
	$(CC) $(CFLAGS) $(INCLUDE) ev.o src/common.c src/list.c src/net.c src/ev_cb.c src/relay.c -o bin/relay

test: test_queue
	$(CC) test/becho.c -o bin/becho

test_queue: mkdir
	$(CC) $(CFLAGS) $(INCLUDE) src/list.c test/test_queue.c -o bin/test_queue

clean:
	rm -f *.o
	rm -f bin/relay bin/test*
	rm -f bin/becho
