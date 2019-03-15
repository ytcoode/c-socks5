#ifndef _S5_UTIL_EPOLL_H
#define _S5_UTIL_EPOLL_H

#include <stdbool.h>

#define S5_EPOLL_ITEM_SET_STATE(p, state) \
  s5_epoll_item_set_state(p, state, #state)

typedef struct s5_epoll s5_epoll_t;
typedef void (*s5_epoll_item_hdlr)(void *p, s5_epoll_t *ep);

s5_epoll_t *s5_epoll_create();
void s5_epoll_destroy(s5_epoll_t *ep);

int s5_epoll_add(s5_epoll_t *ep, int fd, void *p);
int s5_epoll_del(s5_epoll_t *ep, int fd);
int s5_epoll_run(s5_epoll_t *ep);

void *s5_epoll_item_create(int size, s5_epoll_item_hdlr *hdlrs, int state,
                           s5_epoll_t *ep);
void s5_epoll_item_destroy(void *p, s5_epoll_t *ep);

bool s5_epoll_item_is_state(void *p, int state);
void s5_epoll_item_set_state(void *p, int state, char *state_s);

#endif
