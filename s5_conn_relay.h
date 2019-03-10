#ifndef _S5_CONN_RELAY_H
#define _S5_CONN_RELAY_H

#include "s5_conn.h"

bool h_relay_in(void* p, s5_epoll_t* ep);
bool h_relay_out(void* p, s5_epoll_t* ep);

#endif
