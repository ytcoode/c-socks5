#ifndef _S5_CONN_NEGOTIATE_H
#define _S5_CONN_NEGOTIATE_H

#include "s5_type.h"
#include "s5_util.h"

void h_select_method_req(void *p, s5_epoll_t *ep);
void h_select_method_reply(void *p, s5_epoll_t *ep);

void h_connect_req(void *p, s5_epoll_t *ep);
void h_target_connected(void *p, s5_epoll_t *ep);

void s5_connect_target(s5_conn_t *p, uint32_t ip, s5_epoll_t *ep);

#endif
