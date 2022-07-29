#include <vector>
#include <utility>
#include <string>
#include <arpa/nameser.h>
#include <resolv.h>
#include <uv.h>

#define TYPE_UDP 1
#define TYPE_TCP 2
#define TYPE_DOH 3

using namespace std;
// Type can be TCP, UDP or DOH (TCP can be used with TLS)
// Guarantees that all ids are unique
int generate_dns_queries(uint16_t number, int type, vector<uv_buf_t> *result, string &query_name);

#define GET_TCP_QUERY_HEADER_ADDR(tcp_msg_ptr) ((char *)tcp_msg_ptr + 2)
#define GET_DNS_MSG_ID(dns_msg_ptr) ((uint16_t)*dns_msg_ptr)
#define GET_DNS_MSG_QDCOUNT(dns_msg_ptr) (uint16_t)*(((char *)dns_msg_ptr) + 2)
#define GET_DNS_MSG_ANCOUNTCOUNT(dns_msg_ptr) (uint16_t)*(((char *)dns_msg_ptr) + 3)
#define GET_DNS_MSG_NSCOUNT(dns_msg_ptr) (uint16_t)*(((char *)dns_msg_ptr) + 4)
#define GET_DNS_MSG_ARCOUNT(dns_msg_ptr) (uint16_t)*(((char *)dns_msg_ptr) + 5)

#define GET_DNS_MSG_FLAGS_ADDR(dns_msg_addr) ((char *)dns_msg_ptr + 2)
#define GET_DNS_MSG_FLAGS_QR(dns_msg_flags_ptr) (((uint16_t)*dns_msg_flags_ptr) & 1)
#define GET_DNS_MSG_FLAGS_OPCODE(dns_msg_flags_ptr) ((((uint16_t)*dns_msg_flags_ptr) >> 1) & ((1 << 4) - 1))
#define GET_DNS_MSG_FLAGS_RCODE(dns_msg_flags_ptr) ((((uint16_t)*dns_msg_flags_ptr) >> 12) & ((1 << 4) - 1))

#define DNS_RCODE_NOERROR 0
#define DNS_RCODE_FMTERROR 1
#define DNS_RCODE_SERVFAIL 2
#define DNS_RCODE_NAMEERROR 3
#define DNS_RCODE_NOTIMPLEMENTED 4
#define DNS_RCODE_REFUSED 5
