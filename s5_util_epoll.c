#include "s5_util_epoll.h"
#include <limits.h>
#include <stdlib.h>
#include <sys/epoll.h>
#include "s5_util_log.h"
#include "s5_util_misc.h"

#define EP_WAIT_N 32
#define EP_IN (EPOLLIN | EPOLLHUP | EPOLLERR)
#define EP_OUT (EPOLLOUT | EPOLLHUP | EPOLLERR)
#define EP_EI_STATE_DESTROYED INT_MIN

typedef struct s5_epoll_item s5_epoll_item_t;

struct s5_epoll {
  int fd;
  uintptr_t ei_free_l;
};

struct s5_epoll_item {
  s5_epoll_item_hdlr *hdlrs;
  int state;
  uintptr_t ei_free_l;
};

s5_epoll_t *s5_epoll_create() {
  s5_epoll_t *ep = malloc(sizeof(s5_epoll_t));
  if (!ep) {
    return NULL;
  }
  ep->fd = epoll_create1(0);
  if (ep->fd == -1) {
    perror("epoll_create1");
    free(ep);
    return NULL;
  }
  ep->ei_free_l = 0;
  return ep;
}

void s5_epoll_destroy(s5_epoll_t *ep) {
  assert(!ep->ei_free_l);
  s5_close(ep->fd);
  free(ep);
}

int s5_epoll_add(s5_epoll_t *ep, int fd, void *p) {
  struct epoll_event ev;
  ev.events = EP_IN | EP_OUT | EPOLLET;
  ev.data.ptr = (s5_epoll_item_t *)p - 1;
  return epoll_ctl(ep->fd, EPOLL_CTL_ADD, fd, &ev);
}

int s5_epoll_del(s5_epoll_t *ep, int fd) {
  return epoll_ctl(ep->fd, EPOLL_CTL_DEL, fd, NULL);
}

static void handle_event(struct epoll_event *ev, s5_epoll_t *ep) {
  s5_epoll_item_t *ei = ev->data.ptr;
  s5_epoll_item_hdlr h;
  void *p = ei + 1;

  if (ei->hdlrs && (ev->events & EP_IN)) {
    h = ei->hdlrs[ei->state * 2];
    if (h) {
      h(p, ep);
    }
  }

  if (ei->hdlrs && (ev->events & EP_OUT)) {
    h = ei->hdlrs[ei->state * 2 + 1];
    if (h) {
      h(p, ep);
    }
  }
}

int s5_epoll_run(s5_epoll_t *ep) {
  struct epoll_event events[EP_WAIT_N];
  s5_epoll_item_t *ei;
  int n, i;

  for (;;) {
    n = epoll_wait(ep->fd, events, EP_WAIT_N, -1);
    if (n == -1) {
      if (errno == EINTR) {
        continue;  // TODO handle signals
      }
      perror_and_exit("epoll_wait");
    }

    for (i = 0; i < n; i++) {
      handle_event(events + i, ep);
    }

    while (ep->ei_free_l) {
      ei = (s5_epoll_item_t *)ep->ei_free_l;
      ep->ei_free_l = ei->ei_free_l;
      free(ei);
    }
  }
  return 0;
}

void *s5_epoll_item_create(int size, s5_epoll_item_hdlr *hdlrs, int state,
                           s5_epoll_t *ep) {
  void *p;
  s5_epoll_item_t *ei = malloc(sizeof(s5_epoll_item_t) + size);
  if (!ei) {
    return NULL;
  }
  ei->hdlrs = hdlrs;
  ei->state = state;
  ei->ei_free_l = 0;
  p = ei + 1;
  S5_DEBUG("s5_epoll_item_create: %p\n", p);
  return p;
}

void s5_epoll_item_destroy(void *p, s5_epoll_t *ep) {
  s5_epoll_item_t *ei = (s5_epoll_item_t *)p - 1;
  assert(!ei->ei_free_l);

  S5_DEBUG("s5_epoll_item_destroy: %p\n", p);

  if (!ep) {
    free(ei);
    return;
  }
  ei->hdlrs = NULL;  // 确保方法不会再被调用
  ei->state = EP_EI_STATE_DESTROYED;
  ei->ei_free_l = ep->ei_free_l;
  ep->ei_free_l = (uintptr_t)ei;
}

bool s5_epoll_item_is_state(void *p, int state) {
  s5_epoll_item_t *ei = (s5_epoll_item_t *)p - 1;
  return ei->state == state;
}

void s5_epoll_item_set_state(void *p, int state, char *state_s) {
  assert(state != EP_EI_STATE_DESTROYED);
  s5_epoll_item_t *ei = (s5_epoll_item_t *)p - 1;
  ei->state = state;
  S5_DEBUG("s5_epoll_item_set_state: %p -> %s\n", p, state_s);
}
