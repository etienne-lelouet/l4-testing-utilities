#pragma once
#include <uv.h>
#include <stdio.h>
#include <vector>
#include <utility>
#include <string>
#include "argsparser.h"
#include "sender.h"

using namespace std;

// class Tcpclient_close : public Sender
// {
// public:
// 	ArgumentParser args;
// 	vector<uv_buf_t> *queries;
// 	uv_loop_t *loop;
// 	struct sockaddr_in *dest;
// 	uv_timer_t *total_timer;
// 	uv_timer_t *delay_timer;
// 	uv_tcp_t *socket;
// 	vector<int> sent;
// 	int index_in_query_list;

// 	Tcpclient_close(vector<uv_buf_t> *queries, struct sockaddr_in *dest, ArgumentParser &args, uv_loop_t *loop);
// 	void run();
// 	void stop();
// };
// #define handle_tcpclient_close(handle) ((Tcpclient_close *)(handle->data))

class Tcpclient_keepalive : public Sender
{
public:
	ArgumentParser *args;
	vector<uv_buf_t> *queries;
	uv_loop_t *loop;
	struct sockaddr_in *dest;
	uv_timer_t *total_timer;
	uv_timer_t *delay_timer;
	uv_tcp_t *socket;
	vector<int> sent;
	int index_in_query_list;
	string canary;
	Tcpclient_keepalive(ArgumentParser *args, vector<uv_buf_t> *queries, struct sockaddr_in *dest, uv_loop_t *loop);
	void run();
	void stop();
};
#define handle_tcpclient_keepalive(handle) ((Tcpclient_keepalive *)(handle->data))

void timer_cb_delay_keepalive(uv_timer_t *handle);
void on_write_cb_keepalive(uv_write_t *req, int status);
void alloc_cb_keepalive(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf);
void on_connect_cb_keepalive(uv_connect_t *req, int status);
void read_cb_keepalive(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf);
void on_tcp_shutdown_keepalive(uv_shutdown_t *req, int status);
