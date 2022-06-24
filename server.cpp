#include <stdio.h>
#include <stdlib.h>
#include <uv.h>
#include <netinet/in.h>
#include <vector>
#include <algorithm>

using namespace std;

// struct linger l = {
//     .l_onoff = 1,
//     .l_linger = 0,
// };
// setsockopt(scom, SOL_SOCKET, SO_LINGER, &l, sizeof(l))

vector<uv_handle_t *> connection_list;
uv_tcp_t server;
int stop_accepting = 0;

static void on_close_cb(uv_handle_t *handle)
{
	auto it = find(connection_list.begin(), connection_list.end(), handle);
	if (it != connection_list.end())
	{
		free(*it);
		connection_list.erase(it);
	}
}

static void sigint_handler(int sig)
{
	stop_accepting = 1;
	for (auto it : connection_list)
	{
		puts("closing active_connections");
		uv_tcp_close_reset((uv_tcp_t *)it, on_close_cb);
	}
}

void read_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
{
	if (nread == UV_EOF)
	{
		uv_close((uv_handle_t *)stream, on_close_cb);
	}
	free(buf->base);
}

void alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
	buf->base = (char *)malloc(suggested_size);
	buf->len = suggested_size;
}

void on_connection_cb(uv_stream_t *server, int status)
{
	printf("connection request received");
	uv_tcp_t *client = (uv_tcp_t *)malloc(sizeof(uv_tcp_t));
	uv_accept(server, (uv_stream_t *)client);
	if (stop_accepting > 0)
	{
		uv_tcp_close_reset(client, on_close_cb);
	}
	connection_list.push_back((uv_handle_t *)client);
	uv_read_start((uv_stream_t *)client, alloc_cb, read_cb);
}

int main(int argc, char **argv)
{
	if (signal(SIGINT, sigint_handler) == SIG_ERR)
		printf("error");
	
	struct sockaddr_in addr;
	uv_loop_t *loop = uv_default_loop();
	uv_tcp_init(loop, &server);
	uv_ip4_addr("0.0.0.0", 6969, &addr);
	uv_tcp_bind(&server, (const struct sockaddr *)&addr, 0);
	uv_listen((uv_stream_t *)&server, 10000, on_connection_cb);

	uv_loop_close(loop);
	return 0;
}