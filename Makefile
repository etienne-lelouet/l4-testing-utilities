CC=g++
REQS=-lresolv -lm
PKG_CONFIG_REQS=libuv
CFLAGS=-c -Wall 
LDFLAGS=`pkg-config --libs $(PKG_CONFIG_REQS)` $(REQS)

TARGETS=server tcpclient udpclient

OBJECTS=server.o tcpclient.o udpclient.o argsparser.o

all: CFLAGS+=-O3
all: $(TARGETS)

debug: CFLAGS+=-g -DDEBUG -O0
debug: $(TARGETS)

server.o: server.cpp
	$(CC) $^ $(CFLAGS) -o $@

tcpclient.o: tcpclient.cpp
	$(CC) $^ $(CFLAGS) -o $@

udpclient.o: udpclient.cpp
	$(CC) $^ $(CFLAGS) -o $@

argsparser.o: argsparser.cpp argsparser.h
	$(CC) $< $(CFLAGS) -o $@

server:	server.o argsparser.o
	$(CC) $^ $(LDFLAGS) -o $@
	sudo setcap CAP_NET_BIND_SERVICE=+eip $@

tcpclient: tcpclient.o argsparser.o
	$(CC) $^ $(LDFLAGS) -o $@

udpclient: udpclient.o argsparser.o
	$(CC) $^ $(LDFLAGS) -o $@

clean:
	rm -rf $(TARGETS) $(OBJECTS)

.PHONY: all