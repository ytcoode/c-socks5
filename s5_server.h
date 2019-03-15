#ifndef _S5_SERVER_H
#define _S5_SERVER_H

#include "s5_type.h"
#include "s5_util.h"

struct s5_server {
  int port;
  int fd;
  int dn2ip_fd_in_n;
  int dn2ip_fd_in_i;
  s5_dn2ip_fd_in_t **dn2ip_fd_in;
  s5_dn2ip_fd_out_t *dn2ip_fd_out;
};

s5_server_t *s5_server_create(int port, s5_epoll_t *ep);
void s5_server_destroy(s5_server_t *s, s5_epoll_t *ep);

int s5_server_start(s5_server_t *s, s5_epoll_t *ep);

#endif
