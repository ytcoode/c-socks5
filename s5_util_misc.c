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

static void signal_handler(int signum) {
  printf("ignore signal: %d\n", signum);
}

int signal_ignore(int signum) {
  struct sigaction sa;
  int err;

  sa.sa_handler = signal_handler;
  sa.sa_flags = 0;

  err = sigfillset(&sa.sa_mask);
  if (err) {
    return -1;
  }

  err = sigaction(signum, &sa, NULL);
  if (err) {
    return -1;
  }
  return 0;
}
