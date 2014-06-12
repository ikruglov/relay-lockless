CC=gcc
CFLAGS=-std=gnu99 -O3 -g -Wall -pthread -DDEBUG -DEV_STANDALONE=1 -fno-strict-aliasing
TSAN=-fsanitize=thread -fsanitize-blacklist=blacklist.tsan -fPIE -pie # need clang for compilation
INCLUDE=-I src -I libev

all: relay

mkdir:
	@mkdir -p bin

relay: mkdir
	$(CC) -c -O3 -g -DEV_STANDALONE=1 -Wno-all libev/ev.c -o ev.o
	$(CC) $(CFLAGS) $(INCLUDE) ev.o src/list.c src/net.c src/server_ctx.c src/client_ctx.c src/background_ctx.c src/relay.c -o bin/relay

tsan: mkdir
	$(CC) -c -O3 -g -fPIC -DEV_STANDALONE=1 -Wno-all libev/ev.c -o ev.o
	$(CC) $(CFLAGS) $(INCLUDE) $(TSAN) ev.o src/list.c src/net.c src/server_ctx.c src/client_ctx.c src/background_ctx.c src/relay.c -o bin/relay

test: test_queue
	$(CC) test/becho.c -o bin/becho

test_queue: mkdir
	$(CC) $(CFLAGS) $(INCLUDE) src/list.c test/test_queue.c -o bin/test_queue

tclient: mkdir
	$(CC) $(CFLAGS) $(INCLUDE) test/tclient.c src/net.c -o bin/tclient


clean:
	rm -f *.o
	rm -f bin/relay bin/test* bin/tclient
	rm -f bin/becho
