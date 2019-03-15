#include "s5_conn_negotiate.h"
#include <string.h>
#include "s5_conn.h"
#include "s5_conn_relay.h"
#include "s5_dns_resolver.h"
#include "s5_server.h"

void h_select_method_req(void *p, s5_epoll_t *ep) {
  s5_conn_t *c = p;
  int n, val;

read:
  n = s5_buf_read(c->buf, c->fd);
  if (n == 0) {
  close:
    s5_conn_destroy(c, ep);
    return;
  }

  if (n == -1) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return;
    }
    if (errno == EINTR) {
      goto read;
    }
    perror("s5_buf_read");
    goto close;
  }

  assert(n > 0);

  /*

  +----+----------+----------+
  |VER | NMETHODS | METHODS  |
  +----+----------+----------+
  | 1  |    1     | 1 to 255 |
  +----+----------+----------+

  */

  n = s5_buf_readable_bytes(c->buf);
  if (n < 2) {
    goto read;
  }

  val = s5_buf_get_int8(c->buf, 1);
  if (n < val + 2) {
    goto read;
  }

  val = s5_buf_read_int8(c->buf);
  if (val != 5) {  // socks version 5
    printf("illegal socks version: %d\n", val);
    goto close;
  }

  val = s5_buf_read_int8(c->buf);
  if (val < 1 || val > 255) {  // nmethods
    printf("illegal nmethods: %d\n", val);
    goto close;
  }

  for (int i = 0; i < val; i++) {
    s5_buf_read_int8(c->buf);
    // TODO method
  }

  /*

  +----+--------+
  |VER | METHOD |
  +----+--------+
  | 1  |   1    |
  +----+--------+

  */

  s5_buf_write_int8(c->peer->buf, 5);  // version
  s5_buf_write_int8(c->peer->buf, 0);  // NO AUTHENTICATION REQUIRED // TODO

  S5_EPOLL_ITEM_SET_STATE(c, SELECT_METHOD_REPLY);
  h_select_method_reply(p, ep);
}

void h_select_method_reply(void *p, s5_epoll_t *ep) {
  s5_conn_t *c = p;
  int n;

write:
  n = s5_buf_write(c->peer->buf, c->fd);
  if (n == -1) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return;
    }
    if (errno == EINTR) {
      goto write;
    }
    perror("s5_buf_write");
    s5_conn_destroy(c, ep);
    return;
  }

  if (s5_buf_readable_bytes(c->peer->buf) > 0) {
    goto write;
  }

  S5_EPOLL_ITEM_SET_STATE(c, CONNECT_REQ);
  h_connect_req(c, ep);
}

void h_connect_req(void *p, s5_epoll_t *ep) {
  s5_conn_t *c = p;
  int n, val, atyp, rbytes;
  uint32_t ip;

  /*

  +----+-----+-------+------+----------+----------+
  |VER | CMD |  RSV  | ATYP | DST.ADDR | DST.PORT |
  +----+-----+-------+------+----------+----------+
  | 1  |  1  | X'00' |  1   | Variable |    2     |
  +----+-----+-------+------+----------+----------+

  */

restart:
  rbytes = s5_buf_readable_bytes(c->buf);
  if (rbytes < 4) {
    goto read;
  }

  atyp = s5_buf_get_int8(c->buf, 3);
  switch (atyp) {
    case 1:  // ipv4
      n = 10;
      break;
    case 3:  // domain name
      if (rbytes < 5) {
        goto read;
      }
      val = s5_buf_get_int8(c->buf, 4);
      n = 7 + val;
      break;
    case 4:  // ipv6
      n = 22;
      break;
    default:
      printf("illegal atyp: %d\n", atyp);
      goto close;
  }

  if (rbytes < n) {
    goto read;
  }

  val = s5_buf_read_int8(c->buf);
  if (val != 5) {  // VER
    goto close;    // TODO err handling
  }

  val = s5_buf_read_int8(c->buf);
  if (val != 1) {  // CMD
    printf("illegal cmd: %d\n", val);
    goto close;
  }

  val = s5_buf_read_int8(c->buf);
  if (val != 0) {  // RSV
    goto close;
  }

  s5_buf_skip_bytes(c->buf, 1);  // atyp

  if (atyp == 1) {  // ipv4
    ip = s5_buf_read_int32(c->buf);
    s5_connect_target(c, ip, ep);
    return;
  }

  if (atyp == 3) {  // domain name
    S5_EPOLL_ITEM_SET_STATE(c, RESOLVE_DOMAIN_NAME);
    if (s5_dn2ip_write_req_or_link(c->s, c)) {
      goto close;
    }
    return;
  }

  // ipv6
  assert(atyp == 4);
  S5_ERROR("ipv6 not support\n");
  goto close;

read:
  n = s5_buf_read(c->buf, c->fd);
  if (n > 0) {
    goto restart;
  }

  if (n == -1) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return;
    }
    if (errno == EINTR) {
      goto read;
    }
    perror("s5_buf_read");
  }

close:
  s5_conn_destroy(c, ep);
}

void s5_connect_target(s5_conn_t *c, uint32_t ip, s5_epoll_t *ep) {
  int err;
  uint16_t port;

  port = s5_buf_read_int16(c->buf);

  int ipb1 = (ip >> 24) & 0xff;  // TODO
  int ipb2 = (ip >> 16) & 0xff;
  int ipb3 = (ip >> 8) & 0xff;
  int ipb4 = (ip >> 0) & 0xff;

  S5_DEBUG("connect: %d.%d.%d.%d:%d\n", ipb1, ipb2, ipb3, ipb4, port);

  c->peer->fd = s5_tcp_nonblocking_socket();
  if (c->peer->fd == -1) {
    perror("s5_tcp_nonblocking_socket");
    goto close;
  }

  err = s5_connect(c->peer->fd, ip, port);
  if (err && errno != EINPROGRESS) {
    perror("s5_connect");
    goto close;
  }

  S5_EPOLL_ITEM_SET_STATE(c->peer, CONNECT_TARGET);

  err = s5_epoll_add(ep, c->peer->fd, c->peer);
  if (err) {
    perror("s5_epoll_add");
    goto close;
  }
  return;

close:
  s5_conn_destroy(c->peer, ep);
}

void h_target_connected(void *p, s5_epoll_t *ep) {
  s5_conn_t *c = p;
  int val;

  if (s5_getsockopt_so_error(c->fd, &val)) {
    perror("s5_getsockopt_so_error");
    goto close;
  }

  if (val != 0) {
    printf("connect_target SO_ERROR: %s\n", strerror(val));
    goto close;  // TODO tell client the error
  }

  /*

  +----+-----+-------+------+----------+----------+
  |VER | REP |  RSV  | ATYP | BND.ADDR | BND.PORT |
  +----+-----+-------+------+----------+----------+
  | 1  |  1  | X'00' |  1   | Variable |    2     |
  +----+-----+-------+------+----------+----------+

  */

  s5_buf_write_int8(c->buf, 5);
  s5_buf_write_int8(c->buf, 0);
  s5_buf_write_int8(c->buf, 0);
  s5_buf_write_int8(c->buf, 1);
  s5_buf_write_int32(c->buf, 0);  // TODO ip
  s5_buf_write_int16(c->buf, 0);  // TODO port

  S5_EPOLL_ITEM_SET_STATE(c, RELAY);
  S5_EPOLL_ITEM_SET_STATE(c->peer, RELAY);

  h_relay_in(c->peer, ep);

  if (s5_epoll_item_is_state(c, RELAY)) {
    h_relay_in(c, ep);
  }
  return;

close:
  s5_conn_destroy(c, ep);
}
