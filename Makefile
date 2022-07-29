CC=g++
REQS_CFLAGS=
REQS_LDFLAGS=-lresolv -lm
PKG_CONFIG_REQS=libuv gnutls
CFLAGS=-c -Wall `pkg-config --cflags $(PKG_CONFIG_REQS)` $(REQS_CFLAGS)
LDFLAGS=`pkg-config --libs $(PKG_CONFIG_REQS)` $(REQS_LDFLAGS)

TARGETS=client

OBJECTS=

all: CFLAGS+=-O3
all: $(TARGETS)

debug: CFLAGS+=-g -DDEBUG -O0
debug: $(TARGETS)

client: client.o argsparser.o tcpclient_keepalive.o dns_utils.o
	$(CC) $^ $(LDFLAGS) -o $@

udpclient: udpclient.o argsparser.o
	$(CC) $^ $(LDFLAGS) -o $@

%.o: %.cpp
	$(CC) $< $(CFLAGS) -o $@

server.o: server.cpp
	$(CC) $< $(CFLAGS) -o $@

clean:
	rm -rf $(TARGETS) *.o

.PHONY: all