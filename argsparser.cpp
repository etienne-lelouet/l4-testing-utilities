#include "argsparser.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>

using namespace std;

ArgumentParser::ArgumentParser() : ip("127.0.0.1"), port(6000), ms_delay_between_batch(1000), n_queries_per_batch(1), n_sockets(1), total_duration_ms(10000), query_name("jeanpierre.moe"), max_queries_per_socket(10), max_queries_total(10) {}

ArgumentParser::ArgumentParser(ArgumentParser &old) : ip(old.ip), port(old.port), ms_delay_between_batch(old.ms_delay_between_batch), n_queries_per_batch(old.n_queries_per_batch), n_sockets(old.n_sockets), total_duration_ms(old.total_duration_ms), query_name(old.query_name), max_queries_per_socket(old.max_queries_per_socket), max_queries_total(old.max_queries_total) {}

int ArgumentParser::parse_arguments(int argc, char **argv)
{
	int opt;
	char *endptr;

	while ((opt = getopt(argc, argv, ":i:p:n:N:s:D:H:")) != -1)
	{
		switch (opt)
		{
		case 'i':
			ip = optarg;
			break;
		case 'p':
			this->port = strtoul(optarg, &endptr, 0);
			if (endptr == optarg || port >= (1 << 16) || port == 0)
			{
				printf("invalid value provided to option %c\n", opt);
				return -1;
			}
			break;
		case 'n':
			this->n_sockets = strtoul(optarg, &endptr, 0);
			if (endptr == optarg)
			{
				printf("invalid value provided to option %c\n", opt);
				return -1;
			}
			break;
		case 'N':
			this->n_queries_per_batch = strtoul(optarg, &endptr, 0);
			if (endptr == optarg)
			{
				printf("invalid value provided to option %c\n", opt);
				return -1;
			}
		case 's':
			this->ms_delay_between_batch = strtoul(optarg, &endptr, 0);
			if (endptr == optarg)
			{
				printf("invalid value provided to option %c\n", opt);
				return -1;
			}
			break;
		case 'D':
			this->total_duration_ms = strtoul(optarg, &endptr, 0);
			if (endptr == optarg)
			{
				printf("invalid value provided to option %c\n", opt);
				return -1;
			}
			break;
		case 'H':
			query_name = optarg;
			break;
		case ':':
			printf("option -%c needs a value\n", optopt);
			break;
		case '?':
			printf("unknown option : -%c\n", optopt);
			break;
		}
	}
	max_queries_per_socket = (long)trunc((double)(this->total_duration_ms / this->ms_delay_between_batch)) * this->n_queries_per_batch;
	max_queries_total = max_queries_per_socket * this->n_sockets;
	return 0;
}
void ArgumentParser::print_arguments()
{
	this->print_arguments(stdout);
}

void ArgumentParser::print_arguments(FILE *stream)
{
	fprintf(stream, "Querying %s:%hd, with %lu queries every %lu milliseconds, from %lu sockets, for %lu seconds (maximum %lu queries on one socket, %lu queries total)\n", this->ip.c_str(), this->port, this->n_queries_per_batch, this->ms_delay_between_batch, this->n_sockets, this->total_duration_ms, this->max_queries_per_socket, this->max_queries_total);
}