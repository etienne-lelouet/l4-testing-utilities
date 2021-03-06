#include <stdio.h>
#include <stdlib.h>
#include <uv.h>
#include <signal.h>
#include <vector>
#include <algorithm>
#include <string>
#include <arpa/nameser.h>
#include <resolv.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#include <assert.h>
#include <unistd.h>
#include <ctime>

#include "argsparser.h"

using namespace std;

#define CHECK(x) assert((x) >= 0)
#define LOOP_CHECK(rval, cmd)                                         \
	do                                                                \
	{                                                                 \
		rval = cmd;                                                   \
	} while (rval == GNUTLS_E_AGAIN || rval == GNUTLS_E_INTERRUPTED); \
	assert(rval >= 0)

gnutls_certificate_credentials_t session_creds;
gnutls_session_t session;

ArgumentParser args;
int n_current_connections = 0;
int n_initiated_connections = 0;

uv_timer_t timer;
struct sockaddr_in dest;
uv_tcp_t *tcp_sock;
char *dns_msg;
size_t dns_msg_len;
char *tcp_msg;
size_t tcp_msg_len;
string recv_buffer;
enum ConnectionStatus
{
	NOTHING_E,
	TCP_ESTAB_E,
	TLS_HANDSHAKE_E,
	TLS_ESTAB_E,
	TLS_CLOSING_E,
	TCP_CLOSING_E
};

ConnectionStatus connstat = NOTHING_E;

void on_close_opened_conn(uv_handle_t *handle)
{
	puts("on_close_opened_conn");
	free(tcp_sock);
}
void on_tcp_shutdown(uv_shutdown_t *req, int status)
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

void on_fail_open(uv_handle_t *handle)
{
	puts("on_fail_open");
	free(tcp_sock);
}

void timer_cb_send(uv_timer_t *handle)
{
	puts("timer_cb, writing");
	ssize_t sent = gnutls_record_send(session, tcp_msg, tcp_msg_len);
	if (sent < 0)
	{
		printf("Error in sending data: %s", gnutls_strerror(sent));
	}
}

void tls_conn_cb()
{
	puts("tls_conn_cb");
	uv_timer_start(&timer, timer_cb_send, 0, args.ms_delay_between_batch);
}

void tls_handshake()
{
	int err = gnutls_handshake(session);
	if (err == GNUTLS_E_SUCCESS)
	{
		puts("GNUTLS_E_SUCCESS");
		connstat = TLS_ESTAB_E;
		tls_conn_cb();
	}
	else if (err < 0 && gnutls_error_is_fatal(err))
	{
		printf("gnutls handshake fatal error : %s\n", gnutls_strerror(err));
	}
	else if (err != GNUTLS_E_AGAIN && err != GNUTLS_E_INTERRUPTED)
	{
		printf("other handshake error : %s\n", gnutls_strerror(err));
	}
}

void read_cb(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buf)
{
	if (nread == UV_EOF)
	{
		puts("read UV_EOF");
		if (connstat == TLS_ESTAB_E || connstat == TLS_CLOSING_E)
		{
			puts("read UV_EOF, closing");
			uv_read_stop((uv_stream_t *)stream);
			uv_close((uv_handle_t *)stream, on_close_opened_conn);
		}
	}
	else if (nread < 0)
	{
		printf("read_cb error : %s, closing connection\n", uv_strerror(nread));
	}
	else if (nread > 0)
	{
		printf("nread : %lu \n", nread);
		recv_buffer.append(buf->base, nread);
		switch (connstat)
		{
		case NOTHING_E:
			puts("NOTHING_E");
			break;
		case TCP_ESTAB_E:
			puts("TCP_ESTAB_E");
			break;
		case TLS_CLOSING_E:
			puts("TLS_CLOSING_E");
			break;
		case TCP_CLOSING_E:
			puts("TCP_CLOSING_E");
			break;
		case TLS_HANDSHAKE_E:
			puts("handshake, receiving data");
			tls_handshake();
			break;
		case TLS_ESTAB_E:
			puts("estab, receiving data");
			for (;;)
			{
				char gnutls_buf[16384];
				ssize_t len = gnutls_record_recv(session, gnutls_buf, sizeof(buf));
				if (len > 0)
				{
					printf("received and decoded tls record\n");
					// TODO interpret as dns response
				}
				else
				{
					if (len == GNUTLS_E_AGAIN)
					{
						// Check if we don't have any data left to read
						if (recv_buffer.empty())
						{
							break;
						}
						continue;
					}
					else if (len == GNUTLS_E_INTERRUPTED)
					{
						continue;
					}
					break;
				}
			}
		}
	}
	puts("freeing buf");
	free(buf->base);
}

void on_write_cb(uv_write_t *req, int status)
{
	if (status < 0)
	{
		printf("on_write_cb error : %s\n", uv_strerror(status));
	}
	puts("writer cb, freeing req");
	printf("req->data %p \n", req->data);
	free(req->data);
	free(req);
}

void alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf)
{
	buf->base = (char *)malloc(suggested_size);
	buf->len = suggested_size;
}

void on_connect_cb(uv_connect_t *req, int status)
{
	puts("setting status to TLS_HANDSHAKE_E");
	connstat = TLS_HANDSHAKE_E;
	uv_read_start(req->handle, alloc_cb, read_cb);
	tls_handshake();
	// establish gnutls connection
	free(req);
}

void shutdown_client(int sig)
{
	uv_timer_stop(&timer);
	puts("shutdown client");
	connstat = TLS_CLOSING_E;
	int err = gnutls_bye(session, GNUTLS_SHUT_WR);
	if (err == GNUTLS_E_SUCCESS)
	{
		puts("gnutls_bye success");
	}
	else
	{
		printf("gnutls_bye error : %s\n", gnutls_strerror(err));
	}
	puts("stoping reads");
	uv_shutdown_t *shutdown_req = (uv_shutdown_t *)malloc(sizeof(uv_shutdown_t));
	puts("shutting down write end");
	int result = uv_shutdown(shutdown_req, (uv_stream_t *)tcp_sock, on_tcp_shutdown);
	printf("uv_shutdown result = %s", uv_strerror(result));
}

static ssize_t gnutls_pull_trampoline(gnutls_transport_ptr_t h, void *buf, size_t len)
{
	puts("gnutls_pull_trampoline");
	if (!recv_buffer.empty())
	{
		puts("gnutls_pull_trampoline not empty");
		len = std::min(len, recv_buffer.size());
		memcpy(buf, recv_buffer.data(), len);
		recv_buffer.erase(0, len);
		return len;
	}

	errno = EAGAIN;
	return -1;
}

static ssize_t gnutls_push_trampoline(gnutls_transport_ptr_t h, const void *buf, size_t len)
{
	puts("gnutls_push_trampoline");
	uv_write_t *req = (uv_write_t *)malloc(sizeof(uv_write_t));
	char *data = (char *)malloc(len);
	req->data = data;
	printf("req->data %p \n", req->data);
	memcpy(data, buf, len);
	/*
	cette fonction est sens??e reprendre la s??mantique de write, donc j'imagine que le pointeur pass?? en arg
	reste valide jusqu'?? ce qu'on retourne de la fonction, donc trop t??t
	(puisque quand on retourne dans la fonction on a aucune garantie que les donn??es ont ??t?? envoy??es)
	donc on malloc + memcpy et on fera un free dans le callback de write
	*/
	struct uv_buf_t uv_buf = uv_buf_init(data, len);
	uv_write(req, (uv_stream_t *)tcp_sock, &uv_buf, 1, on_write_cb);

	return len;
}

int main(int argc, char **argv)
{
	printf("pid: %d\n", getpid());
	if (signal(SIGINT, shutdown_client) == SIG_ERR)
	{
		perror("signal");
		exit(1);
	}
	args = ArgumentParser();
	if (args.parse_arguments(argc, argv) != 0)
	{
		return -1;
	}
	int err = uv_ip4_addr(args.ip.c_str(), args.port, &dest);
	if (err < 0)
	{
		printf("uv_ip4_addr error : %s\n", uv_strerror(err));
		exit(0);
	}
	args.print_arguments();

	if (gnutls_check_version("3.4.6") == NULL)
	{
		fprintf(stderr, "GnuTLS 3.4.6 or later is required for this example\n");
		exit(1);
	}

	/* for backwards compatibility with gnutls < 3.3.0 */
	CHECK(gnutls_global_init());

	/* X509 stuff */
	CHECK(gnutls_certificate_allocate_credentials(&session_creds));

	/* sets the system trusted CAs for Internet PKI */
	CHECK(gnutls_certificate_set_x509_system_trust(session_creds));

	CHECK(gnutls_init(&session, GNUTLS_CLIENT | GNUTLS_NONBLOCK));
	CHECK(gnutls_set_default_priority(session));
	CHECK(gnutls_credentials_set(session, GNUTLS_CRD_CERTIFICATE, session_creds));

	gnutls_transport_set_pull_function(session, gnutls_pull_trampoline);
	gnutls_transport_set_push_function(session, gnutls_push_trampoline);
	gnutls_handshake_set_timeout(session, GNUTLS_DEFAULT_HANDSHAKE_TIMEOUT);
	int query_len;
	dns_msg = (char *)malloc(PACKETSZ),
	query_len = res_mkquery(QUERY, args.query_name.c_str(), ns_c_in, ns_t_a, NULL, 0, NULL, (unsigned char *)dns_msg, PACKETSZ);
	if (query_len <= 0)
	{
		puts("error creating the dns_msg wih res_mkquery");
		free(dns_msg);
	}
	else
	{
		dns_msg_len = (size_t)query_len;
		tcp_msg_len = dns_msg_len + 2;
		tcp_msg = (char *)malloc(tcp_msg_len);
		uint16_t plen = htons(query_len);
		memcpy(tcp_msg, &plen, sizeof(plen));
		memcpy(tcp_msg + 2, dns_msg, dns_msg_len);
		free(dns_msg);
	}
	connstat = NOTHING_E;
	tcp_sock = (uv_tcp_t *)malloc(sizeof(uv_tcp_t));
	uv_loop_t *loop = uv_default_loop();
	uv_tcp_init(loop, tcp_sock);
	uv_connect_t *connect = (uv_connect_t *)malloc(sizeof(uv_connect_t));
	uv_tcp_connect(connect, tcp_sock, (const struct sockaddr *)&dest, on_connect_cb);
	uv_timer_init(loop, &timer);
	uv_run(loop, UV_RUN_DEFAULT);
	int close_res = uv_loop_close(loop);
	if (close_res == EAGAIN)
	{
		puts("close_res == EAGAIN");
	}
	free(tcp_msg);

	gnutls_deinit(session);
	gnutls_certificate_free_credentials(session_creds);
	gnutls_global_deinit();
	return 0;
}
