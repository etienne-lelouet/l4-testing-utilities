CC=g++
CFLAGS=-Wall -g `pkg-config --cflags --libs libuv`

TARGETS=server client

all: $(TARGETS)

server:	server.cpp
	$(CC) $^ $(CFLAGS) -o $@

client: client.cpp
	$(CC) $^ $(CFLAGS) -o $@

clean:
	rm -rf $(TARGETS)

.PHONY: all