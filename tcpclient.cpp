#include <stdio.h>
#include <stdlib.h>
#include <uv.h>
#include <signal.h>
#include <vector>
#include <algorithm>
#include <string>
#include <unistd.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>

using namespace std;

#define N_MAX_PARALLEL_CONNECTIONS 2

string ip;
string hostname;
unsigned long port;
int delay_ms;
int N_QUERIES;
uv_timer_cb timer_cb;
unsigned long n_max_parallel_connections;
int n_current_connections = 0;
int n_initiated_connections = 0;
uv_loop_t *loop;
uv_timer_t timer;
struct sockaddr_in dest;
vector<uv_handle_t *> initiated_connections;
vector<tuple<uv_handle_t *, int>> active_connections;
char *dns_msg;
size_t dns_msg_len;
char *tcp_msg;
size_t tcp_msg_len;

void on_close_opened_conn(uv_handle_t *handle)
{
	if (active_connections.empty())
	{
		puts("error, active_connections is empty before removal");
	}
	auto it = find_if(active_connections.begin(), active_connections.end(), [handle](tuple<uv_handle_t *, int> &t)
					  { return get<0>(t) == handle; });
	if (it != active_connections.end())
	{
#ifdef DEBUG
		puts("found in list of opened connections, freeing and removing from vec");
#endif
		free(get<0>(*it));
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
		printf("read_cb error : %s, closing connection\n", uv_strerror(nread));
	}
	if (nread > 0)
	{
		printf("received data of len %lu on stream %p : %s\n", buf->len, stream, buf->base);
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
		printf("on_write_cb error : %s, closing connection\n", uv_strerror(status));
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
	active_connections.push_back(make_tuple((uv_handle_t *)req->handle, 0));
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

	for (auto it = active_connections.begin(); it != active_connections.end();
		 it++)
	{
#ifdef DEBUG
		puts("timer_cb, writing");
#endif
		if (get<1>(*it) < N_QUERIES)
		{
			printf("timer tick : seent %d / %d queries\n", get<1>(*it), N_QUERIES);
			uv_write_t *req = (uv_write_t *)malloc(sizeof(uv_write_t));
			struct uv_buf_t buf = uv_buf_init(tcp_msg, tcp_msg_len);
			uv_write(req, (uv_stream_t *)get<0>(*it), &buf, 1, on_write_cb);
			get<1>(*it) = get<1>(*it) + 1;
			printf("timer tick : seent %d / %d queries\n", get<1>(*it), N_QUERIES);
		}
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
		uv_close(get<0>(it), on_close_opened_conn);
	}
}

int main(int argc, char **argv)
{
	ip = "127.0.0.1";
	hostname = "jeanpierre.moe";
	port = 6969;
	delay_ms = 1000;
	N_QUERIES = 1;
	timer_cb = timer_cb_nosend;
	n_max_parallel_connections = N_MAX_PARALLEL_CONNECTIONS;
	int opt;
	while ((opt = getopt(argc, argv, ":i:p:n:s:N:H:")) != -1)
	{
		puts("ici");
		switch (opt)
		{
		case 'i':
			puts("i");
			ip = optarg;
			break;
		case 'p':
			puts("p");
			port = strtoul(optarg, NULL, 0);
			break;
		case 'n':
			puts("n");
			n_max_parallel_connections = strtoul(optarg, NULL, 0);
			break;
		case 's':
			puts("s");
			timer_cb = timer_cb_send;
			delay_ms = atoi(optarg);
			break;
		case 'N':
			puts("N");
			N_QUERIES = atoi(optarg);
			break;
		case 'H':
			puts("H");
			hostname = optarg;
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
		printf("port number must be striclty superior to 0 and strinctly inferior to %d, is %lu \n", (1 << 16), port);
	}
	loop = uv_default_loop();
	int err = uv_ip4_addr(ip.c_str(), port, &dest);
	printf("client querying addr %s and port %lu \n", ip.c_str(), port);
	if (err < 0)
	{
		printf("uv_ip4_addr error : %s\n", uv_strerror(err));
		exit(0);
	}
	int query_len;
	if (timer_cb == timer_cb_send)
	{
		dns_msg = (char *)malloc(PACKETSZ),
		query_len = res_mkquery(QUERY, hostname.c_str(), ns_c_in, ns_t_a, NULL, 0, NULL, (unsigned char *)dns_msg, PACKETSZ);
		if (query_len <= 0)
		{
			puts("error creating the dns_msg wih res_mkquery");
			free(dns_msg);
			goto end;
		}
		else
		{
			dns_msg_len = (size_t)query_len;
			tcp_msg_len = dns_msg_len + 2;
			tcp_msg = (char *)malloc(tcp_msg_len);
			// write pkt len
			uint16_t plen = htons(query_len);
			memcpy(tcp_msg, &plen, sizeof(plen));
			// write wire
			memcpy(tcp_msg + 2, dns_msg, dns_msg_len);
			// // write id requested
			// uint16_t _id = ntohs(id);
			// memcpy(buf.get() + 2 + offset, &_id, sizeof(_id));
			free(dns_msg);
		}
	}
	uv_timer_init(loop, &timer);
	uv_timer_start(&timer, timer_cb, 0, delay_ms);
	uv_run(loop, UV_RUN_DEFAULT);
end:
	uv_loop_close(loop);
	return 0;
}
