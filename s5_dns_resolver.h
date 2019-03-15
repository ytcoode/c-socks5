#ifndef _S5_DNS_RESOLVER_H
#define _S5_DNS_RESOLVER_H

#include "s5_type.h"
#include "s5_util.h"

int s5_dns_resolver_start(s5_server_t *s, s5_epoll_t *ep);

int s5_dn2ip_write_req_or_link(s5_server_t *fin,
                               s5_conn_t *c);  // 0 ok, -1 err
int s5_dn2ip_write_req(s5_dn2ip_fd_in_t *fin,
                       s5_conn_t *c);  // 0 eagain, 1 ok, -1 err

void h_dn2ip_write_req(void *p, s5_epoll_t *ep);
void h_dn2ip_read_reply(void *p, s5_epoll_t *ep);

#endif
