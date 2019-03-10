#ifndef _S5_CONN_NEGOTIATE_H
#define _S5_CONN_NEGOTIATE_H

#include "s5_conn.h"

bool h_select_method_req(void *p, s5_epoll_t *ep);
bool h_select_method_reply(void *p, s5_epoll_t *ep);
bool h_connect_req(void *p, s5_epoll_t *ep);
bool h_connect_target(void *p, s5_epoll_t *ep);

#endif
