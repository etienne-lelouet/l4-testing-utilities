CC=g++
CFLAGS=-Wall `pkg-config --cflags --libs libuv`

TARGETS=server tcpclient

all: CFLAGS+=-O3
all: $(TARGETS)

debug: CFLAGS+=-g -DDEBUG -O0
debug: $(TARGETS)

server:	server.cpp
	$(CC) $^ $(CFLAGS) -o $@

tcpclient: tcpclient.cpp
	$(CC) $^ $(CFLAGS) -o $@

clean:
	rm -rf $(TARGETS)

.PHONY: all