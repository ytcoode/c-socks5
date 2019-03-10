#include "s5_conn_relay.h"
#include <sys/socket.h>
#include "s5_conn_shutdown.h"

bool h_relay_in(void* p, s5_epoll_t* ep) {
  s5_conn_t* c = p;
  int n, err;

  n = s5_buf_copy(c->buf, c->fd, c->peer->fd);
  if (n > 0) {
    return false;
  }

  if (n == -1) {
    return true;
  }

  err = shutdown(c->peer->fd, SHUT_WR);
  if (err) {
    perror("shutdown");
    return true;
  }

  s5_epoll_item_set_hdlr(c, h_shutdown, h_shutdown, "shutdown");
  s5_epoll_item_set_hdlr(c->peer, h_shutdown, h_shutdown, "shutdown");
  return h_shutdown(p, ep);
}

bool h_relay_out(void* p, s5_epoll_t* ep) {
  return h_relay_in(((s5_conn_t*)p)->peer, ep);
}
