#include "s5_util_misc.h"
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

void s5_close(int fd) {
  int err = close(fd);
  if (err) {
    perror("close");
  }
}

int s5_fcntl_set_nonblock(int fd) {
  int flags = fcntl(fd, F_GETFL, 0);
  if (flags == -1) {
    return -1;
  }
  return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}
