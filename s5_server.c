#include "s5_server.h"
#include <fcntl.h>
#include <unistd.h>
#include "s5_conn.h"

static int handle_conn(int fd, s5_epoll_t *ep) {
  int err = s5_fcntl_set_nonblock(fd);
  if (err) {
    perror("s5_fcntl_set_nonblock");
    return -1;
  }

  s5_conn_t *c = s5_conn_create(fd);
  if (c == NULL) {
    return -1;
  }

  err = s5_epoll_add(ep, fd, c);
  if (err) {
    return -1;
  }
  return 0;
}

static bool h_accept(void *p, s5_epoll_t *ep) {
  s5_server_t *s = (s5_server_t *)p;
  for (;;) {
    int fd = s5_accept(s->fd);
    if (fd == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        break;  // TODO some err like eagain
      }
      if (errno == EINTR) {
        continue;  // TODO handle signal
      }
      perror_and_exit("s5_accept");
    }

    if (handle_conn(fd, ep)) {
      s5_close(fd);
      continue;
    }
    printf("accept new conn: %d\n", fd);
  }
  return false;
}

static void s5_server_destroy(void *p) {
  s5_server_t *s = p;
  if (s->fd >= 0) {
    s5_close(s->fd);
  }
  s5_epoll_item_free(s);
}

s5_server_t *s5_server_create(int port) {
  s5_server_t *s = s5_epoll_item_alloc(sizeof(s5_server_t));
  if (s) {
    s5_epoll_item_init(s, h_accept, NULL, NULL, s5_server_destroy);
    s->port = port;
    s->fd = -1;
  }
  return s;
}

int s5_server_listen(s5_server_t *s) {
  s->fd = s5_listen(s->port);
  if (s->fd == -1) {
    return -1;
  }
  printf("listen: %d\n", s->port);
  return 0;
}
