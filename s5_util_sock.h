#ifndef _S5_UTIL_SOCK_H
#define _S5_UTIL_SOCK_H

int s5_tcp_nonblocking_socket();

int s5_listen(int port);
int s5_accept(int fd);
int s5_connect(int fd, int ip, int port);

int s5_getsockname(int fd, int *ip, int *port);
int s5_getsockopt_so_error(int fd, int *val);
int s5_get_ip_by_domain(char *domain, int *ip);

#endif
