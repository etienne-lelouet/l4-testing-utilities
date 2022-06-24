CC=g++
CFLAGS=-Wall -g `pkg-config --cflags --libs libuv`

TARGETS=server tcpclient

all: $(TARGETS)

server:	server.cpp
	$(CC) $^ $(CFLAGS) -o $@

tpcclient: tcpclient.cpp
	$(CC) $^ $(CFLAGS) -o $@

clean:
	rm -rf $(TARGETS)

.PHONY: all