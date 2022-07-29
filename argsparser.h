#pragma once
#include <string>
#include <stdio.h>
class ArgumentParser
{
public:
	std::string ip;
	unsigned short port;
	long ms_delay_between_batch;
	long n_queries_per_batch;
	long n_sockets;
	long total_duration_ms;
	std::string query_name;
	unsigned long max_queries_per_socket;
	unsigned long max_queries_total;
	ArgumentParser();
	ArgumentParser(ArgumentParser &old);

	int parse_arguments(int argc, char **argv);
	void print_arguments();
	void print_arguments(FILE *stream);
};
