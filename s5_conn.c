#include "s5_conn.h"
#include "s5_conn_negotiate.h"
#include "s5_conn_relay.h"

#define BUF_SIZE 4096  // 必须要足够大到放下每条negotiation消息
#define BUF_SIZE_MIN (256 * 2 + 1)  // Username/Password Authentication Req

static void s5_conn_close(void *p) {
  s5_conn_t *c = p;
  s5_epoll_item_null_hdlr(c);
  s5_epoll_item_null_hdlr(c->peer);
}

static void s5_conn_destroy0(s5_conn_t *c) {
  if (c->fd >= 0) {
    s5_close(c->fd);
    c->fd = -1;
  }

  if (c->buf) {
    s5_buf_destroy(c->buf);
    c->buf = NULL;
  }

  c->peer = NULL;
  s5_epoll_item_free(c);
}

static void s5_conn_destroy(void *p) {
  s5_conn_t *c1 = p;
  s5_conn_t *c2 = c1->peer;

  assert(c2->peer == c1);
  S5_DEBUG("s5_conn_destroy: %p, %d -> %d\n", p, c1->fd, c2->fd);

  s5_conn_destroy0(c1);
  s5_conn_destroy0(c2);
}

static s5_conn_t *s5_conn_create0(int fd) {
  s5_conn_t *c = s5_epoll_item_alloc(sizeof(s5_conn_t));
  if (!c) {
    return NULL;
  }

  s5_epoll_item_init(c, h_select_method_req, NULL, s5_conn_close,
                     s5_conn_destroy);
  c->fd = fd;

  assert(BUF_SIZE >= BUF_SIZE_MIN);
  c->buf = s5_buf_create(BUF_SIZE);
  if (!c->buf) {
    s5_epoll_item_free(c);
    return NULL;
  }

  c->peer = NULL;
  return c;
}

s5_conn_t *s5_conn_create(int fd) {
  s5_conn_t *c1 = s5_conn_create0(fd);
  if (!c1) {
    return NULL;
  }

  s5_conn_t *c2 = s5_conn_create0(-1);
  if (!c2) {
    s5_conn_destroy0(c1);
    return NULL;
  }

  c1->peer = c2;
  c2->peer = c1;
  return c1;
}
