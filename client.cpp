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
#include "argsparser.h"
#include "sender.h"
#include "tcpclient.h"
#include "dns_utils.h"

using namespace std;
vector<Tcpclient_keepalive> *senders;

#define N_MAX_PARALLEL_CONNECTIONS 2

void shutdown_client(int sig)
{
	exit(1);
	for (auto sender : *senders)
	{
		sender.stop();
	}
}

int main(int argc, char **argv)
{
	ArgumentParser args;
	struct sockaddr_in dest;
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
	args.print_arguments();

	uv_loop_t *loop = uv_default_loop();
	vector<vector<uv_buf_t> *> *all_queries = new vector<vector<uv_buf_t> *>();
	all_queries->reserve(args.n_sockets);
	senders = new vector<Tcpclient_keepalive>();
	for (int i = 0; i < args.n_sockets; i++)
	{
		puts("generate queries and create sender");
		vector<uv_buf_t> *queries = new vector<uv_buf_t>();
		queries->reserve(args.max_queries_total);
		if (generate_dns_queries(args.max_queries_per_socket, TYPE_TCP, queries, args.query_name) != 0)
		{
			puts("error in generate_dns_queries");
			exit(1);
		}

		all_queries->push_back(queries);
		senders->emplace_back(args, queries, &dest, loop);
	}
	for (auto sender : *senders)
	{
		puts("running sender");
		sender.run();
	}
	uv_run(loop, UV_RUN_DEFAULT);
	uv_loop_close(loop);
	puts("after loop");
	return 0;
}
