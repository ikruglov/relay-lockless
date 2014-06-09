CC=gcc
CFLAGS=-std=gnu99 -O0 -g -Wall -DDEBUG
INCLUDE=-I src
LIBS=-lev

all: relay

relay:
	@mkdir -p bin
	$(CC) $(CFLAGS) $(INCLUDE) $(LIBS) src/list.c src/net.c src/ev_cb.c src/relay.c -o bin/relay

test: test_queue

test_queue:
	@mkdir -p bin
	$(CC) $(CFLAGS) $(INCLUDE) src/list.c test/test_queue.c -o bin/test_queue

clean:
	rm -rf bin
