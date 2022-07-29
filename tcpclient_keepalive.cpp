#include "tcpclient.h"
#include "dns_utils.h"
#include <algorithm>
#include "argsparser.h"
#include <vector>
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
	puts("timer_cb_delay_keepalive");
	printf("canary manuel = %s\n", ((Tcpclient_keepalive *)(handle->data))->canary.c_str());
	Tcpclient_keepalive *this_sender = handle_tcpclient_keepalive(handle);
	printf("canary macro = %s\n", this_sender->canary.c_str());
	if (this_sender->sent.size() > 0)
	{
		printf("%lu queries in flight at timer cb \n", this_sender->sent.size());
		this_sender->sent.clear();
	}
	vector<uv_buf_t> &queries = *this_sender->queries;
	uv_write_t *req = (uv_write_t *)malloc(sizeof(uv_write_t));
	int res = uv_write(req, (uv_stream_t *)(this_sender->socket), &(queries[this_sender->index_in_query_list]), this_sender->args.n_queries_per_batch, on_write_cb_keepalive);
	if (res != 0)
	{
		printf("uv_write error : %s,\n", uv_strerror(res));
	}
}

void on_write_cb_keepalive(uv_write_t *req, int status)
{
	Tcpclient_keepalive *this_sender = handle_tcpclient_keepalive(req->handle);
	if (status < 0)
	{
		printf("on_write_cb error : %s, closing connection\n", uv_strerror(status));
		// uv_close((uv_handle_t *)req->handle, on_close_opened_conn);
	}
	vector<uv_buf_t> &queries = *this_sender->queries;
	for (int i = this_sender->index_in_query_list; i < this_sender->index_in_query_list + this_sender->args.n_queries_per_batch; i++)
	{
		printf("just sent id : %d\n", GET_DNS_MSG_ID(GET_TCP_QUERY_HEADER_ADDR(queries[i].base)));
		this_sender->sent.push_back(GET_DNS_MSG_ID(GET_TCP_QUERY_HEADER_ADDR(queries[i].base)));
	}
	this_sender->args.n_queries_per_batch++;
	uv_timer_start(handle_tcpclient_keepalive(req->handle)->delay_timer, timer_cb_delay_keepalive, this_sender->args.ms_delay_between_batch, 0);
	free(req);
}

void alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
	buf->base = (char *)malloc(suggested_size);
	buf->len = suggested_size;
}

void read_cb_keepalive(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
{
	if (nread == UV_EOF)
	{
		puts("read UV_EOF");
		// close connection and then do what ?
		uv_close((uv_handle_t *)stream, on_close_opened_conn);
		return;
	}
	else if (nread < 0)
	{
		printf("read_cb error : %s, closing connection\n", uv_strerror(nread));
	}
	if (nread > 0)
	{
		// Check if valid dns
		// if valid dns => check if query matches sent during this time frame
		// if matches => incr recv_counter, if recv_counter == sent_counter => stop reading
		int query_id = GET_DNS_MSG_ID(buf->base);
		printf("received response to query id %d, RCODE is %d \n", GET_DNS_MSG_ID(buf->base), GET_DNS_MSG_FLAGS_RCODE(buf->base));
		if (std::find(handle_tcpclient_keepalive(stream)->sent.begin(), handle_tcpclient_keepalive(stream)->sent.end(), query_id) != handle_tcpclient_keepalive(stream)->sent.end())
		{
			handle_tcpclient_keepalive(stream)->sent.erase(remove(handle_tcpclient_keepalive(stream)->sent.begin(), handle_tcpclient_keepalive(stream)->sent.end(), query_id));
		}
		else
		{
			printf("query %d timeout \n", query_id);
		}
	}
	if (buf->base)
	{
		free(buf->base);
	}
}

void on_connect_cb_keepalive(uv_connect_t *req, int status)
{
	puts("on connect");
	if (status != 0)
	{
		puts("connection failed");
		if (!uv_is_closing((uv_handle_t *)req->handle))
		{
			// DO smth, I don't know what
		}
		return;
	}
	puts("on_connect_cb_keepalive");
	printf("req->data = %p\n", req->data);
	req->handle->data = req->data;
	printf("req->handle->data = %p\n", req->handle->data);
	uv_read_start(req->handle, alloc_cb, read_cb_keepalive);
	Tcpclient_keepalive *this_sender = (Tcpclient_keepalive *)(req->handle->data);
	uv_timer_start(handle_tcpclient_keepalive(req->handle)->delay_timer, timer_cb_delay_keepalive, this_sender->args.ms_delay_between_batch, 0);
	uv_timer_start(handle_tcpclient_keepalive(req->handle)->total_timer, timer_cb_total, this_sender->args.total_duration_ms, 0);
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

Tcpclient_keepalive::Tcpclient_keepalive(ArgumentParser &argsarg, vector<uv_buf_t> *queriesarg, struct sockaddr_in *destarg, uv_loop_t *looparg) : args(argsarg), queries(queriesarg), loop(looparg), dest(destarg), canary("test")
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
	printf("run, canary : %s\n", this->canary.c_str());
	uv_connect_t *connectrq = (uv_connect_t *)malloc(sizeof(uv_connect_t));
	connectrq->data = (void *)this;
	int err = uv_tcp_connect(connectrq, socket, (const struct sockaddr *)dest, on_connect_cb_keepalive);
	if (err != 0)
	{
		printf("uv_tcp_connect, %s\n", uv_strerror(err));
	}
	printf("run this addr = %p\n", this);
	printf("run socket addr = %p\n", &(this->socket));
}
void Tcpclient_keepalive::stop()
{
	puts("Tcpclient_keepalive::stop()");
	uv_timer_stop(total_timer);
	uv_timer_stop(delay_timer);
	uv_shutdown_t *shutdown_req = (uv_shutdown_t *)malloc(sizeof(uv_shutdown_t));
	uv_shutdown(shutdown_req, (uv_stream_t *)&socket, on_tcp_shutdown_keepalive);
}