#ifndef _S5_UTIL_MISC_H
#define _S5_UTIL_MISC_H

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#define perror_and_exit(msg) \
  do {                       \
    perror(msg);             \
    exit(-1);                \
  } while (0)

#define perror_and_exit2(err, msg) \
  do {                             \
    errno = err;                   \
    perror(msg);                   \
    exit(-1);                      \
  } while (0)

void s5_close(int fd);
int s5_fcntl_set_nonblock(int fd);

#endif
