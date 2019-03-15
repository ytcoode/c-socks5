#include "s5_conn.h"
#include "s5_conn_negotiate.h"
#include "s5_conn_relay.h"
#include "s5_conn_shutdown.h"

#define BUF_SIZE 4096  // 必须要足够大到放下每条negotiation消息
#define BUF_SIZE_MIN (256 * 2 + 1)  // Username/Password Authentication Req

static s5_epoll_item_hdlr hdlrs[] = {
    // SELECT_METHOD_REQ
    h_select_method_req,
    NULL,

    // SELECT_METHOD_REPLY
    NULL,
    h_select_method_reply,

    // CONNECT_REQ
    h_connect_req,
    NULL,

    // RESOLVE_DOMAIN_NAME
    NULL,
    NULL,

    // CONNECT_TARGET
    NULL,
    h_target_connected,

    // CONNECT_REPLY
    NULL,
    NULL,

    // RELAY
    h_relay_in,
    h_relay_out,

    // SHUTDOWN
    h_shutdown,
    h_shutdown,
};

static s5_conn_t *s5_conn_create0(int fd, s5_epoll_t *ep, s5_server_t *s) {
  s5_conn_t *c = s5_epoll_item_create(sizeof(s5_conn_t), hdlrs, -1, ep);
  if (!c) {
    return NULL;
  }

  c->fd = fd;

  assert(BUF_SIZE >= BUF_SIZE_MIN);
  c->buf = s5_buf_create(BUF_SIZE);
  if (!c->buf) {
    s5_epoll_item_destroy(c, ep);
    return NULL;
  }

  c->peer = NULL;
  c->s = s;
  c->dn2ip_req_l = 0;
  return c;
}

static void s5_conn_destroy0(s5_conn_t *c, s5_epoll_t *ep) {
  if (c->fd >= 0) {
    s5_close(c->fd);
    c->fd = -1;
  }
  if (c->buf) {
    s5_buf_destroy(c->buf);
    c->buf = NULL;
  }
  c->peer = NULL;
  c->s = NULL;
  s5_epoll_item_destroy(c, ep);
}

s5_conn_t *s5_conn_create(int fd, s5_epoll_t *ep, s5_server_t *s) {
  s5_conn_t *c1 = s5_conn_create0(fd, ep, s);
  if (!c1) {
    return NULL;
  }

  s5_conn_t *c2 = s5_conn_create0(-1, ep, s);
  if (!c2) {
    s5_epoll_item_destroy(c1, NULL);
    return NULL;
  }

  c1->peer = c2;
  c2->peer = c1;
  return c1;
}

void s5_conn_destroy(s5_conn_t *c, s5_epoll_t *ep) {
  s5_conn_destroy0(c->peer, ep);
  s5_conn_destroy0(c, ep);
}
