#ifndef _S5_SERVER_H
#define _S5_SERVER_H

#include "s5_util.h"

typedef struct {
  int port;
  int fd;
} s5_server_t;

s5_server_t *s5_server_create(int port);
int s5_server_listen(s5_server_t *s);

#endif
