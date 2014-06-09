CC=gcc
CFLAGS=-std=gnu99 -O3 -g -Wall -DDEBUG -DEV_STANDALONE=1 -fno-strict-aliasing
INCLUDE=-I src -I libev

all: relay

relay:
	@mkdir -p bin
	$(CC) -c -O3 -g -DEV_STANDALONE=1 libev/ev.c -o ev.o
	$(CC) $(CFLAGS) $(INCLUDE) ev.o src/list.c src/net.c src/ev_cb.c src/relay.c -o bin/relay

test: test_queue

test_queue:
	@mkdir -p bin
	$(CC) $(CFLAGS) $(INCLUDE) src/list.c test/test_queue.c -o bin/test_queue

clean:
	rm -f *.o
	rm -f bin/relay bin/test*
