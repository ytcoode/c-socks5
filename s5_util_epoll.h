#ifndef _S5_UTIL_EPOLL_H
#define _S5_UTIL_EPOLL_H

#include <stdbool.h>

typedef struct {
  int fd;
} s5_epoll_t;

// true: close -> destroy
typedef bool (*s5_epoll_item_hdlr)(void *p, s5_epoll_t *ep);
typedef void (*s5_epoll_item_close)(void *p);
typedef void (*s5_epoll_item_destroy)(void *p);

s5_epoll_t *s5_epoll_create();
void s5_epoll_destroy(s5_epoll_t *ep);

int s5_epoll_add(s5_epoll_t *ep, int fd, void *p);
int s5_epoll_del(s5_epoll_t *ep, int fd);
int s5_epoll_run(s5_epoll_t *ep);

void *s5_epoll_item_alloc(int size);
void s5_epoll_item_free(void *p);

void s5_epoll_item_init(void *p, s5_epoll_item_hdlr h_in,
                        s5_epoll_item_hdlr h_out, s5_epoll_item_close close,
                        s5_epoll_item_destroy destroy);
void s5_epoll_item_set_hdlr(void *p, s5_epoll_item_hdlr h_in,
                            s5_epoll_item_hdlr h_out, char *state);
void s5_epoll_item_null_hdlr(void *p);

#endif
