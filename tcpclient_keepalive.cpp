#include "tcpclient.h"
#include "dns_utils.h"
#include <algorithm>
#include "argsparser.h"
#include <vector>
#include <string.h>
using namespace std;

void on_close_opened_conn(uv_handle_t *handle)
{
	puts("on_close_opened_conn");
}

void timer_cb_total(uv_timer_t *handle)
{
	handle_tcpclient_keepalive(handle)->stop();
}

void timer_cb_delay_keepalive(uv_timer_t *handle)
{
	Tcpclient_keepalive *this_sender = handle_tcpclient_keepalive(handle);
	printf("%p timer_cb_delay_keepalive\n", this_sender);
	if (this_sender->sent.size() > 0)
	{
		printf("%lu queries in flight at timer cb \n", this_sender->sent.size());
		this_sender->sent.clear();
	}
	vector<uv_buf_t> &queries = *this_sender->queries;
	uv_write_t *req;
	for (int i = 0; i < this_sender->args->n_queries_per_batch; i++)
	{
		req = (uv_write_t *)malloc(sizeof(uv_write_t));
		req->data = &(queries[this_sender->index_in_query_list]);
		int res = uv_write(req, (uv_stream_t *)(this_sender->socket), &(queries[this_sender->index_in_query_list]), 1, on_write_cb_keepalive);
		if (res != 0)
		{
			printf("uv_write error : %s,\n", uv_strerror(res));
			this_sender->stop();
		}
		this_sender->index_in_query_list++;
	}
}

void on_write_cb_keepalive(uv_write_t *req, int status)
{
	Tcpclient_keepalive *this_sender = handle_tcpclient_keepalive(req->handle);
	if (status < 0)
	{
		printf("on_write_cb error : %s, closing connection\n", uv_strerror(status));
		exit(0);
	}
	uint16_t sent_id;
	memcpy(&sent_id, ((uv_buf_t *)(req->data))->base + sizeof(uint16_t), (sizeof(uint16_t)));
	sent_id = ntohs(sent_id);
	printf("just sent id : %d\n", sent_id);
	this_sender->sent.push_back(sent_id);
	if (this_sender->sent.size() == this_sender->args->n_queries_per_batch)
	{
		uv_timer_start(this_sender->delay_timer, timer_cb_delay_keepalive, this_sender->args->ms_delay_between_batch, 0);
	}
	free(req);
}

void alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
	buf->base = (char *)malloc(suggested_size);
	buf->len = suggested_size;
}

void read_cb_keepalive(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
{
	Tcpclient_keepalive *this_sender = handle_tcpclient_keepalive(stream);
	if (nread == UV_EOF)
	{
		puts("read UV_EOF");
		if (!uv_is_closing((uv_handle_t *)stream))
		{
			uv_close((uv_handle_t *)stream, on_close_opened_conn);
		}
		return;
	}
	else if (nread < 0)
	{
		printf("read_cb error : %s, closing connection\n", uv_strerror(nread));
		this_sender->stop();
	}
	ssize_t left_to_read = nread;
	ssize_t read_total;
	char *base = buf->base;
	while (left_to_read > 0)
	{
		uint16_t sz;
		memcpy(&sz, base, (sizeof(uint16_t)));
		sz = ntohs(sz);
		uint16_t recv_id;
		memcpy(&recv_id, base + sizeof(uint16_t), (sizeof(uint16_t)));
		recv_id = ntohs(recv_id);
		if (std::find(this_sender->sent.begin(), this_sender->sent.end(), recv_id) != this_sender->sent.end())
		{
			this_sender->sent.erase(remove(this_sender->sent.begin(), this_sender->sent.end(), recv_id));
		}
		else
		{
			printf("query %d timeout \n", recv_id);
		}
		read_total = sz + sizeof(uint16_t);
		left_to_read = left_to_read - read_total;
		base = base + read_total;
	}
	if (buf->base)
	{
		free(buf->base);
	}
}

void on_connect_cb_keepalive(uv_connect_t *req, int status)
{
	Tcpclient_keepalive *this_sender = handle_tcpclient_keepalive(req->handle);
	printf("%p on_connect_cb_keepalive\n", this_sender);
	if (status != 0)
	{
		puts("connection failed");
		if (!uv_is_closing((uv_handle_t *)req->handle))
		{
			// DO smth, I don't know what
		}
		return;
	}
	req->handle->data = req->data;
	uv_read_start(req->handle, alloc_cb, read_cb_keepalive);
	uv_timer_start(this_sender->delay_timer, timer_cb_delay_keepalive, 0, 0);
	uv_timer_start(this_sender->total_timer, timer_cb_total, this_sender->args->total_duration_ms, 0);
	free(req);
}

void on_tcp_shutdown_keepalive(uv_shutdown_t *req, int status)
{
	puts("on tcp shutdown");
	if (status < 0)
	{
		printf("on_tcp_shutdown error : %s\n", uv_strerror(status));
	}
	if (!uv_is_closing((uv_handle_t *)req->handle))
	{
		uv_close((uv_handle_t *)req->handle, on_close_opened_conn);
	}
	free(req);
}

Tcpclient_keepalive::Tcpclient_keepalive(ArgumentParser *argsargs, vector<uv_buf_t> *queriesarg, struct sockaddr_in *destarg, uv_loop_t *looparg) : args(argsargs), queries(queriesarg), loop(looparg), dest(destarg), canary("test")
{
	total_timer = (uv_timer_t *)malloc(sizeof(uv_timer_t));
	total_timer->data = (void *)this;
	uv_timer_init(loop, total_timer);
	delay_timer = (uv_timer_t *)malloc(sizeof(uv_timer_t));
	delay_timer->data = (void *)this;
	uv_timer_init(loop, delay_timer);
	socket = (uv_tcp_t *)malloc(sizeof(uv_tcp_t));
	int err = uv_tcp_init(loop, socket);
	if (err != 0)
	{
		printf("uv_tcp_init socket, %s\n", uv_strerror(err));
	}
	socket->data = (void *)this;
}

void Tcpclient_keepalive::run()
{
	uv_connect_t *connectrq = (uv_connect_t *)malloc(sizeof(uv_connect_t));
	connectrq->data = (void *)this;
	int err = uv_tcp_connect(connectrq, socket, (const struct sockaddr *)dest, on_connect_cb_keepalive);
	if (err != 0)
	{
		printf("uv_tcp_connect, %s\n", uv_strerror(err));
	}
}
void Tcpclient_keepalive::stop()
{
	puts("Tcpclient_keepalive::stop()");
	uv_timer_stop(total_timer);
	uv_timer_stop(delay_timer);
	uv_shutdown_t *shutdown_req = (uv_shutdown_t *)malloc(sizeof(uv_shutdown_t));
	uv_shutdown(shutdown_req, (uv_stream_t *)socket, on_tcp_shutdown_keepalive);
}