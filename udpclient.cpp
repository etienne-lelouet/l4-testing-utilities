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
#include <arpa/inet.h>
#include <netinet/in.h>

using namespace std;

#define N_MAX_PARALLEL_CONNECTIONS 2

uv_loop_t *loop;
uv_udp_t send_socket;
uv_udp_t recv_socket;

unsigned char *packet;
int query_len;
size_t packet_len;

void alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
	buf->base = (char *)malloc(suggested_size);
	buf->len = suggested_size;
}

void on_send(uv_udp_send_t *req, int status) {
	printf("sent, status : %d\n", status);
}

int main()
{
	string hostname = "jeanpierre.moe";
	packet = (unsigned char *)malloc(PACKETSZ),
	query_len = res_mkquery(QUERY, hostname.c_str(), ns_c_in, ns_t_a, NULL, 0, NULL, (unsigned char *)packet, PACKETSZ);
	if (query_len <= 0)
	{
		puts("error creating the packet wih res_mkquery");
		free(packet);
		return -1;
	}
	else
	{
		packet_len = (size_t)query_len;
		packet = (unsigned char *)realloc(packet, packet_len);
		printf("buf len is %lu, stored at %p\n", packet_len, packet);
	}
	loop = uv_default_loop();

	uv_udp_init(loop, &send_socket);
	struct sockaddr_in addr;
    uv_ip4_addr("127.0.0.1", 0, &addr);
    uv_udp_bind(&send_socket, (const struct sockaddr *)&addr, 0);

	uv_udp_send_t send_req;
	uv_buf_t msg = uv_buf_init((char *)packet, query_len);

	struct sockaddr_in send_addr;
	uv_ip4_addr("127.0.0.1", 53, &send_addr);
	uv_udp_send(&send_req, &send_socket, &msg, 1, (const struct sockaddr *)&send_addr, on_send);

	return uv_run(loop, UV_RUN_DEFAULT);
}