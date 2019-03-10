#include "s5_util_sock.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/tcp.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include "s5_util_log.h"
#include "s5_util_misc.h"

int s5_tcp_nonblocking_socket() {
  return socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, IPPROTO_TCP);
}

static int s5_getsockopt(int fd, int opt, int *val) {
  socklen_t tmp = sizeof(*val);
  return getsockopt(fd, SOL_SOCKET, opt, val, &tmp);
}

int s5_getsockopt_so_error(int fd, int *val) {
  return s5_getsockopt(fd, SO_ERROR, val);
}

static int s5_setsockopt(int fd, int opt, int val) {
  return setsockopt(fd, SOL_SOCKET, opt, &val, sizeof(val));
}

static int s5_setsockopt_so_reuseaddr(int fd) {
  return s5_setsockopt(fd, SO_REUSEADDR, 1);
}

static int s5_setsockopt_tcp(int fd, int opt, int val) {
  return setsockopt(fd, IPPROTO_TCP, opt, &val, sizeof(val));
}

static int s5_setsockopt_tcp_nodelay(int fd) {  // TODO TCP_CORK
  return s5_setsockopt_tcp(fd, TCP_NODELAY, 1);
}

static int s5_bind(int fd, int ip, int port) {
  struct sockaddr_in addr;

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(ip);
  addr.sin_port = htons(port);

  return bind(fd, (struct sockaddr *)&addr, sizeof(addr));
}

int s5_listen(int port) {
  int fd = s5_tcp_nonblocking_socket();
  if (fd == -1) {
    return -1;
  }

  int err = s5_setsockopt_so_reuseaddr(fd);
  if (err) {
    s5_close(fd);
    return -1;
  }

  err = s5_setsockopt_tcp_nodelay(fd);
  if (err) {
    s5_close(fd);
    return -1;
  }

  err = s5_bind(fd, INADDR_ANY, port);
  if (err) {
    s5_close(fd);
    return -1;
  }

  err = listen(fd, 16);
  if (err) {
    s5_close(fd);
    return -1;
  }

  return fd;
}

int s5_accept(int fd) { return accept(fd, NULL, NULL); }

int s5_connect(int fd, int ip, int port) {
  struct sockaddr_in addr;

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(ip);
  addr.sin_port = htons(port);

  return connect(fd, (struct sockaddr *)&addr, sizeof(addr));
}

int s5_getsockname(int fd, int *ip, int *port) {
  struct sockaddr_in addr;
  socklen_t addrlen = sizeof(addr);
  int err = getsockname(fd, (struct sockaddr *)&addr, &addrlen);
  if (err) {
    return err;
  }
  *ip = ntohl(addr.sin_addr.s_addr);
  *port = ntohs(addr.sin_port);
  return 0;
}

int s5_get_ip_by_domain(char *s, int *ip) {  // TODO async
  struct addrinfo hints, *res;
  struct sockaddr_in *addr;
  int err;

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_TCP;
  hints.ai_flags = 0;

  S5_DEBUG("s5_get_ip_by_domain: %s\n", s);

  err = getaddrinfo(s, NULL, &hints, &res);
  if (err) {
    printf("s5_get_ip_by_domain error: %s, %s\n", s,
           gai_strerror(err));  // TODO S5_LOG(ERR, "...");
    return -1;
  }

  err = -1;
  while (res) {
    assert(res->ai_addrlen == sizeof(*addr));
    addr = (struct sockaddr_in *)res->ai_addr;
    *ip = addr->sin_addr.s_addr;
    err = 0;
    break;
  }

  *ip = ntohl(*ip);
  freeaddrinfo(res);
  return err;
}
