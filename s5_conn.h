#ifndef _S5_CONN_H
#define _S5_CONN_H

#include "s5_util.h"

typedef struct s5_conn {
  int fd;
  s5_buf_t *buf;
  struct s5_conn *peer;
} s5_conn_t;

s5_conn_t *s5_conn_create(int fd);

#endif
