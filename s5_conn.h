#ifndef _S5_CONN_H
#define _S5_CONN_H

#include "s5_type.h"
#include "s5_util.h"

struct s5_conn {
  int fd;
  s5_buf_t *buf;
  s5_conn_t *peer;
  s5_server_t *s;
  uintptr_t dn2ip_req_l;
};

enum s5_conn_state {
  SELECT_METHOD_REQ,
  SELECT_METHOD_REPLY,
  CONNECT_REQ,
  RESOLVE_DOMAIN_NAME,
  CONNECT_TARGET,
  CONNECT_REPLY,
  RELAY,
  SHUTDOWN,
};

s5_conn_t *s5_conn_create(int fdc, s5_epoll_t *ep, s5_server_t *s);
void s5_conn_destroy(s5_conn_t *c, s5_epoll_t *ep);

#endif
