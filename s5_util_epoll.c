#include "s5_util_epoll.h"
#include <stdlib.h>
#include <sys/epoll.h>
#include "s5_util_log.h"
#include "s5_util_misc.h"

#define EP_EV_NUM 32
#define EP_IN (EPOLLIN | EPOLLHUP | EPOLLERR)
#define EP_OUT (EPOLLOUT | EPOLLHUP | EPOLLERR)

typedef struct {
  s5_epoll_item_hdlr h_in;
  s5_epoll_item_hdlr h_out;
  s5_epoll_item_close close;
  s5_epoll_item_destroy destroy;
} s5_epoll_item_t;

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
  return ep;
}

void s5_epoll_destroy(s5_epoll_t *ep) {
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

static bool handle_event(struct epoll_event *ev, s5_epoll_t *ep) {
  s5_epoll_item_t *ei = ev->data.ptr;
  void *p = ei + 1;

  if (ei->h_in && (ev->events & EP_IN)) {
    if (ei->h_in(p, ep)) {
      return true;
    }
  }

  if (ei->h_out && (ev->events & EP_OUT)) {
    if (ei->h_out(p, ep)) {
      return true;
    }
  }
  return false;
}

int s5_epoll_run(s5_epoll_t *ep) {
  struct epoll_event events[EP_EV_NUM];
  s5_epoll_item_t *fei[EP_EV_NUM];
  int n, i, j;

  for (;;) {
    n = epoll_wait(ep->fd, events, EP_EV_NUM, -1);
    if (n == -1) {
      if (errno == EINTR) {
        continue;  // TODO handle signals
      }
      perror_and_exit("epoll_wait");
    }

    for (i = 0, j = 0; i < n; i++) {
      if (handle_event(events + i, ep)) {
        fei[j] = events[i].data.ptr;
        if (fei[j]->close) {
          fei[j]->close(fei[j] + 1);
        }
        j++;
      }
    }

    for (i = 0; i < j; i++) {
      fei[i]->destroy(fei[i] + 1);
    }
  }

  return 0;
}

void *s5_epoll_item_alloc(int size) {
  s5_epoll_item_t *ei = malloc(sizeof(s5_epoll_item_t) + size);
  return ei ? ei + 1 : NULL;
}

void s5_epoll_item_free(void *p) { free((s5_epoll_item_t *)p - 1); }

void s5_epoll_item_init(void *p, s5_epoll_item_hdlr h_in,
                        s5_epoll_item_hdlr h_out, s5_epoll_item_close close,
                        s5_epoll_item_destroy destroy) {
  s5_epoll_item_t *ei = (s5_epoll_item_t *)p - 1;
  ei->h_in = h_in;
  ei->h_out = h_out;
  ei->close = close;
  ei->destroy = destroy;
}

void s5_epoll_item_set_hdlr(void *p, s5_epoll_item_hdlr h_in,
                            s5_epoll_item_hdlr h_out, char *state) {
  s5_epoll_item_t *ei = (s5_epoll_item_t *)p - 1;
  ei->h_in = h_in;
  ei->h_out = h_out;
  S5_DEBUG("s5_epoll_item_set_hdlr: %p -> %s\n", p, state);
}

void s5_epoll_item_null_hdlr(void *p) {
  s5_epoll_item_set_hdlr(p, NULL, NULL, "NULL");
}
