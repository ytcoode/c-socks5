#include "s5_conn_relay.h"
#include <sys/socket.h>
#include "s5_conn.h"
#include "s5_conn_shutdown.h"

void h_relay_in(void* p, s5_epoll_t* ep) {
  s5_conn_t* c = p;
  int n, err;

  n = s5_buf_copy(c->buf, c->fd, c->peer->fd);
  if (n > 0) {
    return;
  }

  if (n == -1) {
    goto close;
  }

  err = shutdown(c->peer->fd, SHUT_WR);
  if (err) {
    perror("shutdown");
    goto close;
  }

  S5_EPOLL_ITEM_SET_STATE(c, SHUTDOWN);
  S5_EPOLL_ITEM_SET_STATE(c->peer, SHUTDOWN);
  h_shutdown(p, ep);
  return;

close:
  s5_conn_destroy(c, ep);
}

void h_relay_out(void* p, s5_epoll_t* ep) {
  h_relay_in(((s5_conn_t*)p)->peer, ep);
}
