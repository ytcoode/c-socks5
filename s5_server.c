#include "s5_server.h"
#include <fcntl.h>
#include <unistd.h>
#include "s5_conn.h"
#include "s5_dns_resolver.h"

static int handle_conn(int fd, s5_epoll_t *ep, s5_server_t *s) {
  int err = s5_fcntl_set_nonblock(fd);
  if (err) {
    perror("s5_fcntl_set_nonblock");
    return -1;
  }

  s5_conn_t *c = s5_conn_create(fd, ep, s);
  if (c == NULL) {
    return -1;
  }

  S5_EPOLL_ITEM_SET_STATE(c, SELECT_METHOD_REQ);

  err = s5_epoll_add(ep, fd, c);
  if (err) {
    return -1;
  }
  return 0;
}

static void h_accept(void *p, s5_epoll_t *ep) {
  s5_server_t *s = (s5_server_t *)p;
  for (;;) {
    int fd = s5_accept(s->fd);
    if (fd == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;  // TODO some err like eagain
      }
      if (errno == EINTR) {
        continue;
      }
      perror_and_exit("s5_accept");
    }

    if (handle_conn(fd, ep, s)) {
      s5_close(fd);
      continue;
    }
  }
}

static s5_epoll_item_hdlr hdlrs[] = {
    h_accept,
    NULL,
};

s5_server_t *s5_server_create(int port, s5_epoll_t *ep) {
  s5_server_t *s = s5_epoll_item_create(sizeof(s5_server_t), hdlrs, 0, ep);
  if (!s) {
    return NULL;
  }
  s->port = port;
  s->fd = -1;
  s->dn2ip_fd_in_n = 0;
  s->dn2ip_fd_in_i = 0;
  s->dn2ip_fd_in = NULL;
  s->dn2ip_fd_out = NULL;
  return s;
}

void s5_server_destroy(s5_server_t *s, s5_epoll_t *ep) {
  if (s->fd != -1) {
    s5_close(s->fd);
    s->fd = -1;
  }
  // TODO destory fin & fout?
  s5_epoll_item_destroy(s, ep);
}

int s5_server_start(s5_server_t *s, s5_epoll_t *ep) {
  assert(s->port > 0);
  assert(s->fd == -1);
  assert(s->dn2ip_fd_in);
  assert(s->dn2ip_fd_out);

  s->fd = s5_listen(s->port);
  if (s->fd == -1) {
    return -1;
  }
  if (s5_epoll_add(ep, s->fd, s)) {
    return -1;
  }
  S5_DEBUG("listen: %d\n", s->port);
  return 0;
}
