#include "s5_conn_negotiate.h"
#include <string.h>
#include "s5_conn_relay.h"

#define DOMAIN_LEN 256
static char domain[DOMAIN_LEN];

bool h_select_method_req(void *p, s5_epoll_t *ep) {
  s5_conn_t *c = p;
  bool b;
  int n, val;

restart:
  n = s5_buf_read(c->buf, c->fd);
  if (n == 0) {
    return true;
  }

  if (n == -1) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return false;
    }
    if (errno == EINTR) {
      goto restart;
    }
    perror("s5_buf_read");
    return true;
  }

  /*

  +----+----------+----------+
  |VER | NMETHODS | METHODS  |
  +----+----------+----------+
  | 1  |    1     | 1 to 255 |
  +----+----------+----------+

  */

  b = s5_buf_get_byte(c->buf, &val, 1);
  if (!b || s5_buf_readable_bytes(c->buf) < val + 2) {
    goto restart;
  }

  b = s5_buf_read_byte(c->buf, &val);
  assert(b);

  if (val != 5) {  // socks version 5
    printf("illegal version: %d\n", val);
    return true;
  }

  b = s5_buf_read_byte(c->buf, &val);
  assert(b);

  if (val < 1 || val > 255) {  // nmethods
    printf("illegal nmethods: %d\n", val);
    return true;
  }

  for (int i = 0; i < val; i++) {
    int m;
    b = s5_buf_read_byte(c->buf, &m);
    assert(b);
    // TODO
  }

  /*

  +----+--------+
  |VER | METHOD |
  +----+--------+
  | 1  |   1    |
  +----+--------+

  */

  b = s5_buf_write_byte(c->peer->buf, 5);  // version
  assert(b);

  b = s5_buf_write_byte(c->peer->buf, 0);  // NO AUTHENTICATION REQUIRED // TODO
  assert(b);

  s5_epoll_item_set_hdlr(p, NULL, h_select_method_reply, "select_method_reply");
  return h_select_method_reply(p, ep);
}

bool h_select_method_reply(void *p, s5_epoll_t *ep) {
  s5_conn_t *c = p;
  int n;

restart:
  n = s5_buf_write(c->peer->buf, c->fd);
  if (n == -1) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return false;
    }
    if (errno == EINTR) {
      goto restart;
    }
    perror("s5_buf_write");
    return true;
  }

  if (s5_buf_readable_bytes(c->peer->buf) > 0) {
    goto restart;
  }

  s5_epoll_item_set_hdlr(p, h_connect_req, NULL, "connect_req");
  return h_connect_req(p, ep);
}

bool h_connect_req(void *p, s5_epoll_t *ep) {
  s5_conn_t *c = p;
  bool b;
  int n, val, atyp, ip, port;

  /*

  +----+-----+-------+------+----------+----------+
  |VER | CMD |  RSV  | ATYP | DST.ADDR | DST.PORT |
  +----+-----+-------+------+----------+----------+
  | 1  |  1  | X'00' |  1   | Variable |    2     |
  +----+-----+-------+------+----------+----------+

  */

restart:
  b = s5_buf_get_byte(c->buf, &atyp, 3);
  if (!b) {
    goto read;
  }

  switch (atyp) {
    case 1:  // ipv4
      n = 10;
      break;
    case 3:  // domain name
      b = s5_buf_get_byte(c->buf, &val, 4);
      if (!b) {
        goto read;
      }
      n = 7 + val;
      break;
    case 4:  // ipv6
      n = 22;
      break;
    default:
      printf("illegal atyp: %d\n", atyp);
      return true;
  }

  if (s5_buf_readable_bytes(c->buf) < n) {
    goto read;
  }

  b = s5_buf_read_byte(c->buf, &val);
  assert(b);
  if (val != 5) {  // VER
    return true;
  }

  b = s5_buf_read_byte(c->buf, &val);
  assert(b);
  if (val != 1) {  // CMD
    printf("illegal cmd: %d\n", val);
    return true;
  }

  b = s5_buf_read_byte(c->buf, &val);
  assert(b);
  if (val != 0) {  // RSV
    return true;
  }

  b = s5_buf_read_byte(c->buf, &val);
  assert(b);

  if (val == 1) {
    b = s5_buf_read_int(c->buf, &ip);
    assert(b);
  } else if (val == 3) {
    b = s5_buf_read_byte(c->buf, &n);
    assert(b);
    assert(n < DOMAIN_LEN);

    b = s5_buf_read_bytes(c->buf, domain, n);
    assert(b);
    domain[n] = 0;

    n = s5_get_ip_by_domain(domain, &ip);
    if (n) {
      return true;
    }
  } else {
    assert(val == 4);
    S5_ERROR("ipv6 not support\n");
    return true;
  }

  b = s5_buf_read_short(c->buf, &port);
  assert(b);

  int ipb1 = (ip >> 24) & 0xff;  // TODO
  int ipb2 = (ip >> 16) & 0xff;
  int ipb3 = (ip >> 8) & 0xff;
  int ipb4 = (ip >> 0) & 0xff;

  S5_DEBUG("connect: %d.%d.%d.%d:%d\n", ipb1, ipb2, ipb3, ipb4, port);

  c->peer->fd = s5_tcp_nonblocking_socket();
  if (c->peer->fd == -1) {
    perror("s5_tcp_nonblocking_socket");
    return true;
  }

  val = s5_connect(c->peer->fd, ip, port);
  if (val == -1 && errno != EINPROGRESS) {
    perror("s5_connect");
    return true;
  }

  s5_epoll_item_null_hdlr(c);
  s5_epoll_item_set_hdlr(c->peer, NULL, h_connect_target, "connect_target");

  val = s5_epoll_add(ep, c->peer->fd, c->peer);
  if (val == -1) {
    perror("s5_epoll_add");
    return true;
  }
  return false;

read:
  n = s5_buf_read(c->buf, c->fd);
  if (n > 0) {
    goto restart;
  }

  if (n == -1) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return false;
    }
    if (errno == EINTR) {
      goto read;
    }
    perror("s5_buf_read");
  }

  return true;
}

bool h_connect_target(void *p, s5_epoll_t *ep) {
  s5_conn_t *c = p;
  int val;

  if (s5_getsockopt_so_error(c->fd, &val)) {
    perror("s5_getsockopt_so_error");
    return true;
  }

  if (val != 0) {
    printf("connect_target SO_ERROR: %s\n", strerror(val));
    return true;  // TODO tell client the error
  }

  /*

  +----+-----+-------+------+----------+----------+
  |VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
  +----+-----+-------+------+----------+----------+
  | 1  |  1  | X'00' |  1   | Variable |    2     |
  +----+-----+-------+------+----------+----------+

  */

  if (!s5_buf_write_byte(c->buf, 5)) {
    return true;  // TODO shoud not happen
  }

  if (!s5_buf_write_byte(c->buf, 0)) {
    return true;
  }

  if (!s5_buf_write_byte(c->buf, 0)) {
    return true;
  }

  if (!s5_buf_write_byte(c->buf, 1)) {
    return true;
  }

  if (!s5_buf_write_int(c->buf, 0)) {  // TODO ip
    return true;
  }

  if (!s5_buf_write_short(c->buf, 0)) {  // TODO port
    return true;
  }

  s5_epoll_item_set_hdlr(c, h_relay_in, h_relay_out, "relay");
  s5_epoll_item_set_hdlr(c->peer, h_relay_in, h_relay_out, "relay");

  if (h_relay_in(c, ep)) {
    return true;
  }

  if (h_relay_in(c->peer, ep)) {
    return true;
  }

  return false;
}
