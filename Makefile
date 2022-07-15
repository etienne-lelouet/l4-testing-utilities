CC=g++
REQS_CFLAGS=
REQS_LDFLAGS=-lresolv -lm
PKG_CONFIG_REQS=libuv gnutls
CFLAGS=-c -Wall `pkg-config --cflags $(PKG_CONFIG_REQS)` $(REQS_CFLAGS)
LDFLAGS=`pkg-config --libs $(PKG_CONFIG_REQS)` $(REQS_LDFLAGS)

TARGETS=server tcpclient udpclient tlsclient

OBJECTS=server.o tcpclient.o udpclient.o argsparser.o tlsclient.o

all: CFLAGS+=-O3
all: $(TARGETS)

debug: CFLAGS+=-g -DDEBUG -O0
debug: $(TARGETS)

udpclient: udpclient.o argsparser.o
	$(CC) $^ $(LDFLAGS) -o $@

tlsclient: tlsclient.o argsparser.o
	$(CC) $^ $(LDFLAGS) -o $@

tcpclient: tcpclient.o argsparser.o
	$(CC) $^ $(LDFLAGS) -o $@

server:	server.o argsparser.o
	$(CC) $^ $(LDFLAGS) -o $@
	sudo setcap CAP_NET_BIND_SERVICE=+eip $@

%.o: %.cpp argsparser.h
	$(CC) $< $(CFLAGS) -o $@

server.o: server.cpp
	$(CC) $< $(CFLAGS) -o $@

clean:
	rm -rf $(TARGETS) $(OBJECTS)

.PHONY: all