#include <stdio.h>
#include <stdlib.h>
#include <uv.h>
#include <signal.h>
#include <vector>
#include <algorithm>

using namespace std;

#define N_MAX_PARALLEL_CONNECTIONS 2

int n_current_connections = 0;
int n_initiated_connections = 0;
uv_loop_t *loop;
uv_timer_t timer;
struct sockaddr_in dest;
int stopping = 0;
vector<uv_handle_t *> initiated_connections;
vector<uv_handle_t *> active_connections;

void on_close_opened_conn(uv_handle_t *handle)
{
	if (active_connections.empty())
	{
		puts("error, active_connections is empty before removal");
	}
	auto it = find(active_connections.begin(), active_connections.end(), handle);
	if (it != active_connections.end())
	{
		puts("found in list of opened connections, freeing and removing from vec");
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
		uv_close((uv_handle_t *)stream, on_close_opened_conn);
	}
	free(buf->base);
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
		uv_close((uv_handle_t *)req->handle, on_fail_open);
		return;
	}
	puts("connected");
	auto it = find(initiated_connections.begin(), initiated_connections.end(), (uv_handle_t *)req->handle);
	if (it != initiated_connections.end())
	{
		puts("found in list of initiated connections, removing");
		initiated_connections.erase(it);
	}
	else
	{
		puts("error, handle not found in list of initiated connections");
	}
	active_connections.push_back((uv_handle_t *)req->handle);
	uv_read_start(req->handle, alloc_cb, read_cb);
	free(req);
}

void timer_cb(uv_timer_t *handle)
{
	printf("timer tick : n_initiated_connections = %d, n_current_connections = %d \n", initiated_connections.size(), active_connections.size());
	if (initiated_connections.size() + active_connections.size() >= N_MAX_PARALLEL_CONNECTIONS)
	{
		puts("n_initiated_connections >= N_MAX_PARALLEL_CONNECTIONS");
		return;
	}
	uv_tcp_t *sock = (uv_tcp_t *)malloc(sizeof(uv_tcp_t));
	if (sock == NULL)
	{
	}
	uv_tcp_init(loop, sock);
	uv_connect_t *connect = (uv_connect_t *)malloc(sizeof(uv_connect_t));
	uv_tcp_connect(connect, sock, (const struct sockaddr *)&dest, on_connect_cb);
	initiated_connections.push_back((uv_handle_t *)sock);
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
		puts("closing initiated_connections");
		uv_close(it, on_fail_open);
	}
	for (auto it : active_connections)
	{
		puts("closing active_connections");
		uv_close(it, on_close_opened_conn);
	}
}

int main(int argc, char **argv)
{
	signal(SIGINT, shutdown_client);
	loop = uv_default_loop();
	uv_ip4_addr("127.0.0.1", 6969, &dest);
	uv_timer_init(loop, &timer);
	uv_timer_start(&timer, timer_cb, 0, 1000);
	uv_run(loop, UV_RUN_DEFAULT);
	uv_loop_close(loop);
	return 0;
}
