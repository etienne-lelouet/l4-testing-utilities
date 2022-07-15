# Requirments

glibc, libuv

# How-to

- Compile with `make`
- `server` will listen for tcp connections on port 6969
- `tcpclient` will try to open a set number of connections (`#define N_MAX_PARALLEL_CONNECTIONS`) to `localhost:6969`

# TODO

Support DNS-over-TLS
Support DNS over HTTPS
Multiple kinds of TCP connection managment : all at once, gradually and open / close