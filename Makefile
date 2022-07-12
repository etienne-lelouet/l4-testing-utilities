CC=g++
REQS=libuv
CFLAGS=-Wall `pkg-config --cflags --libs $(REQS)` -lresolv 

TARGETS=server tcpclient udpclient

all: CFLAGS+=-O3
all: $(TARGETS)

debug: CFLAGS+=-g -DDEBUG -O0
debug: $(TARGETS)

server:	server.cpp
	$(CC) $^ $(CFLAGS) -o $@
	sudo setcap CAP_NET_BIND_SERVICE=+eip $@

tcpclient: tcpclient.cpp
	$(CC) $^ $(CFLAGS) -o $@

udpclient: udpclient.cpp
	$(CC) $^ $(CFLAGS) -o $@

clean:
	rm -rf $(TARGETS)

.PHONY: all