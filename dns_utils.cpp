#include <vector>
#include <utility>
#include <string>
#include <arpa/nameser.h>
#include <resolv.h>
#include <uv.h>
#include "dns_utils.h"
#include <string.h>

using namespace std;

int generate_dns_queries(uint16_t number, int type, vector<uv_buf_t> *result, string &query_name)
{
	for (uint16_t id = 0; id < number; id++)
	{
		int query_len;
		char *dns_msg = (char *)malloc(PACKETSZ);
		query_len = res_mkquery(QUERY, query_name.c_str(), ns_c_in, ns_t_a, NULL, 0, NULL, (unsigned char *)dns_msg, PACKETSZ);
		if (query_len <= 0)
		{
			puts("error creating the dns_msg wih res_mkquery");
			free(dns_msg);
			return 1;
		}
		uint16_t id_nbo = htons(id);
		memcpy(dns_msg, &id_nbo, sizeof(id_nbo));
		size_t dns_msg_len = (size_t)query_len;
		if (type == TYPE_TCP)
		{
			size_t tcp_msg_len = dns_msg_len + 2;
			char *tcp_msg = (char *)malloc(tcp_msg_len);
			uint16_t plen = htons(query_len);
			memcpy(tcp_msg, &plen, sizeof(plen));
			memcpy(tcp_msg + 2, dns_msg, dns_msg_len);
			free(dns_msg);
			dns_msg = tcp_msg;
			dns_msg_len = tcp_msg_len;
		}
		else if (type == TYPE_DOH)
		{
			puts("doh not implemented yet !");
			return 1;
		}
		result->push_back(uv_buf_init(dns_msg, dns_msg_len));
	}
	return 0;
}