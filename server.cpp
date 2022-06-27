#include <stdio.h>
#include <stdlib.h>
#include <uv.h>
#include <netinet/in.h>
#include <vector>
#include <algorithm>
#include <string>
#include <unistd.h>

using namespace std;

// struct linger l = {
//     .l_onoff = 1,
//     .l_linger = 0,
// };
// setsockopt(scom, SOL_SOCKET, SO_LINGER, &l, sizeof(l))

uv_loop_t *loop;
vector<uv_handle_t *> connection_list;
uv_tcp_t server;
uv_timer_t timer;

static void on_close_cb(uv_handle_t *req)
{
	puts("in on_close_cb");
	auto it = find(connection_list.begin(), connection_list.end(), (uv_handle_t *)req);
	if (it != connection_list.end())
	{
		puts("found in list, freeing and erasing");
		free(*it);
		connection_list.erase(it);
	}
}

void on_close_server(uv_handle_t *handle)
{
	puts("server shutdown completed, closing active_connections");
	for (auto it : connection_list)
	{
		puts("closing one connection");
		uv_close((uv_handle_t *)it, on_close_cb);
	}
}

static void sigint_handler(int sig)
{
	puts("received sigint, closing server and timer");
	uv_close((uv_handle_t *)&server, on_close_server);
	uv_timer_stop(&timer);
}

void on_write_cb(uv_write_t *req, int status)
{
	if (status < 0)
	{
		printf("on_write_cb error : %s\n", uv_strerror(status));
		uv_close((uv_handle_t *)req->handle, on_close_cb);
	}
	puts("writer cb, freeing req");
	free(req);
}

void read_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
{
	if (nread == UV_EOF)
	{
		puts("read UV_EOF");
		uv_close((uv_handle_t *)stream, on_close_cb);
	}
	else if (nread < 0)
	{
		printf("read_cb error : %s\n", uv_strerror(nread));
		uv_close((uv_handle_t *)stream, on_close_cb);
	}
	if (nread > 0)
	{
		uv_write_t *req = (uv_write_t *)malloc(sizeof(uv_write_t));
		printf("buf->base = %s\n", buf->base);
		uv_write(req, stream, buf, 1, on_write_cb);
	}
}

void alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
	buf->base = (char *)malloc(suggested_size);
	buf->len = suggested_size;
}

void on_connection_cb(uv_stream_t *server, int status)
{
	if (status < 0)
	{
		printf("on_connection_cb error : %s\n", uv_strerror(status));
		return;
	}
	puts("connection request received");
	uv_tcp_t *client = (uv_tcp_t *)malloc(sizeof(uv_tcp_t));
	uv_tcp_init(loop, client);
	uv_accept(server, (uv_stream_t *)client);
	connection_list.push_back((uv_handle_t *)client);
	uv_read_start((uv_stream_t *)client, alloc_cb, read_cb);
}

void timer_cb(uv_timer_t *handle)
{
	printf("%lu opened connections\n", connection_list.size());
}

int main(int argc, char **argv)
{
	int opt;
	string ip = "0.0.0.0";
	long port = 6969;
	while ((opt = getopt(argc, argv, ":i:p:")) != -1)
	{
		switch (opt)
		{
		case 'i':
			ip = optarg;
			break;
		case 'p':
			port = strtoul(optarg, NULL, 0);
			break;
		case ':':
			printf("option -%c needs a value\n", optopt);
			break;
		case '?':
			printf("unknown option : -%c\n", optopt);
			break;
		}
	}
	if (signal(SIGINT, sigint_handler) == SIG_ERR)
	{
		perror("signal");
		exit(1);
	}

	if (port >= (1 << 16) || port == 0)
	{
		printf("port number must be striclty superior to 0 and strinctly inferior to %d, is %ld \n", (1 << 16), port);
	}

	struct sockaddr_in addr;
	loop = uv_default_loop();
	uv_tcp_init(loop, &server);
	int err = uv_ip4_addr(ip.c_str(), port, &addr);
	if (err < 0)
	{
		printf("uv_ip4_addr error : %s\n", uv_strerror(err));
		exit(0);
	}
	err = uv_tcp_bind(&server, (const struct sockaddr *)&addr, 0);
	if (err < 0)
	{
		printf("uv_tcp_bind error : %s\n", uv_strerror(err));
		exit(0);
	}
	err = uv_listen((uv_stream_t *)&server, 100000, on_connection_cb);
	if (err < 0)
	{
		printf("uv_listen error : %s\n", uv_strerror(err));
		exit(0);
	}
	uv_timer_init(loop, &timer);
	uv_timer_start(&timer, timer_cb, 0, 1000);
	uv_run(loop, UV_RUN_DEFAULT);
	uv_loop_close(loop);
	return 0;
}
