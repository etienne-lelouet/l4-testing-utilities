#include <stdio.h>
#include <stdlib.h>
#include <uv.h>
#include <signal.h>
#include <vector>
#include <algorithm>
#include <string>
#include <unistd.h>

using namespace std;

#define N_MAX_PARALLEL_CONNECTIONS 2

unsigned long n_max_parallel_connections;
int n_current_connections = 0;
int n_initiated_connections = 0;
uv_loop_t *loop;
uv_timer_t timer;
struct sockaddr_in dest;
int stopping = 0;
vector<uv_handle_t *> initiated_connections;
vector<uv_handle_t *> active_connections;

uv_buf_t tosend[1] = {{.base = "echo\0",
					   .len = 5}};

void on_close_opened_conn(uv_handle_t *handle)
{
	if (active_connections.empty())
	{
		puts("error, active_connections is empty before removal");
	}
	auto it = find(active_connections.begin(), active_connections.end(), handle);
	if (it != active_connections.end())
	{
#ifdef DEBUG
		puts("found in list of opened connections, freeing and removing from vec");
#endif
		free(*it);
		active_connections.erase(it);
	}
	else
	{
		puts("error, handle not found in list of opened connections");
	}
}

void on_fail_open(uv_handle_t *handle)
{
#ifdef DEBUG
	puts("removing from list");
#endif
	auto it = find(initiated_connections.begin(), initiated_connections.end(), handle);
	if (it != initiated_connections.end())
	{
		free(*it);
		initiated_connections.erase(it);
	}
	else
	{
		puts("error, handle not found in list of initiated connections");
	}
}

void read_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
{
	if (nread == UV_EOF)
	{
#ifdef DEBUG
		puts("read UV_EOF");
#endif
		uv_close((uv_handle_t *)stream, on_close_opened_conn);
		return;
	}
	else if (nread < 0)
	{
		printf("read_cb error : %s\n", uv_strerror(nread));
		uv_close((uv_handle_t *)stream, on_close_opened_conn);
		return;
	}
	if (nread > 0)
	{
		printf("received on %p : %s\n", stream, buf->base);
	}
	if (buf->base)
	{
		free(buf->base);
	}
}

void on_write_cb(uv_write_t *req, int status)
{
	if (status < 0)
	{
		printf("on_write_cb error : %s\n", uv_strerror(status));
		uv_close((uv_handle_t *)req->handle, on_close_opened_conn);
	}
#ifdef DEBUG
	puts("writer cb, freeing req");
#endif
	free(req);
}

void alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
	buf->base = (char *)malloc(suggested_size);
	buf->len = suggested_size;
}

void on_connect_cb(uv_connect_t *req, int status)
{
	if (status != 0)
	{
		puts("connection failed");
		if (!uv_is_closing((uv_handle_t *)req->handle))
		{
			uv_close((uv_handle_t *)req->handle, on_fail_open);
		}
		return;
	}
#ifdef DEBUG
	puts("connected");
#endif
	auto it = find(initiated_connections.begin(), initiated_connections.end(), (uv_handle_t *)req->handle);
	if (it != initiated_connections.end())
	{
#ifdef DEBUG
		puts("found in list of initiated connections, removing");
		#endif
		initiated_connections.erase(it);
	}
	else
	{
#ifdef DEBUG
		puts("error, handle not found in list of initiated connections");
#endif
	}
	active_connections.push_back((uv_handle_t *)req->handle);
	uv_read_start(req->handle, alloc_cb, read_cb);
	free(req);
}

void timer_cb_nosend(uv_timer_t *handle)
{
	printf("timer tick : n_initiated_connections = %lu, n_current_connections = %lu, total_connections : %lu, max_connections : %lu \n", initiated_connections.size(), active_connections.size(), initiated_connections.size() + active_connections.size(), n_max_parallel_connections);
	while ((initiated_connections.size() + active_connections.size()) < n_max_parallel_connections)
	{
		uv_tcp_t *sock = (uv_tcp_t *)malloc(sizeof(uv_tcp_t));
		uv_tcp_init(loop, sock);
		uv_connect_t *connect = (uv_connect_t *)malloc(sizeof(uv_connect_t));
		uv_tcp_connect(connect, sock, (const struct sockaddr *)&dest, on_connect_cb);
		initiated_connections.push_back((uv_handle_t *)sock);
	}
}

void timer_cb_send(uv_timer_t *handle)
{
	printf("timer tick : n_initiated_connections = %lu, n_current_connections = %lu, total_connections : %lu, max_connections : %lu \n", initiated_connections.size(), active_connections.size(), initiated_connections.size() + active_connections.size(), n_max_parallel_connections);
	while ((initiated_connections.size() + active_connections.size()) < n_max_parallel_connections)
	{
		uv_tcp_t *sock = (uv_tcp_t *)malloc(sizeof(uv_tcp_t));
		uv_tcp_init(loop, sock);
		uv_connect_t *connect = (uv_connect_t *)malloc(sizeof(uv_connect_t));
		uv_tcp_connect(connect, sock, (const struct sockaddr *)&dest, on_connect_cb);
		initiated_connections.push_back((uv_handle_t *)sock);
	}

	for (auto it : active_connections)
	{
#ifdef DEBUG
		puts("timer_cb, writing");
#endif
		uv_write_t *req = (uv_write_t *)malloc(sizeof(uv_write_t));
		uv_write(req, (uv_stream_t *)it, tosend, 1, on_write_cb);
	}
}

void shutdown_client(int sig)
{
	printf("signal %d\n", sig);
	uv_timer_stop(&timer);
	if (sig > 0)
	{
		printf("In shutdown from signal\n");
	}
	else
	{
		printf("called shutdown manually\n");
	}
	for (auto it : initiated_connections)
	{
#ifdef DEBUG
		puts("closing initiated_connections");
#endif
		uv_close(it, on_fail_open);
	}
	for (auto it : active_connections)
	{
#ifdef DEBUG
		puts("closing active_connections");
#endif
		uv_close(it, on_close_opened_conn);
	}
}

int main(int argc, char **argv)
{
	int opt;
	string ip = "0.0.0.0";
	long port = 6969;
	uv_timer_cb timer_cb = timer_cb_nosend;
	n_max_parallel_connections = N_MAX_PARALLEL_CONNECTIONS;
	while ((opt = getopt(argc, argv, ":i:p:n:s")) != -1)
	{
		switch (opt)
		{
		case 'i':
			ip = optarg;
			break;
		case 'p':
			port = strtoul(optarg, NULL, 0);
			break;
		case 'n':
			n_max_parallel_connections = strtoul(optarg, NULL, 0);
			break;
		case 's':
			timer_cb = timer_cb_send;
			break;
		case ':':
			printf("option -%c needs a value\n", optopt);
			break;
		case '?':
			printf("unknown option : -%c\n", optopt);
			break;
		}
	}
	if (signal(SIGINT, shutdown_client) == SIG_ERR)
	{
		perror("signal");
		exit(1);
	}

	if (port >= (1 << 16) || port == 0)
	{
		printf("port number must be striclty superior to 0 and strinctly inferior to %d, is %ld \n", (1 << 16), port);
	}
	loop = uv_default_loop();
	int err = uv_ip4_addr(ip.c_str(), port, &dest);
	if (err < 0)
	{
		printf("uv_ip4_addr error : %s\n", uv_strerror(err));
		exit(0);
	}
	uv_timer_init(loop, &timer);
	uv_timer_start(&timer, timer_cb, 0, 1000);
	uv_run(loop, UV_RUN_DEFAULT);
	uv_loop_close(loop);
	return 0;
}
