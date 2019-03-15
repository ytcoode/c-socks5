#include "s5_signal.h"
#include <signal.h>
#include "s5_util.h"

static int s5_signal(int signum, void (*sighdlr)(int)) {
  struct sigaction sa;
  int err;

  sa.sa_handler = sighdlr;
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

void s5_signal_init() {
  int err;
  err = s5_signal(SIGPIPE, SIG_IGN);
  if (err) {
    perror_and_exit("s5_signal");
  }
}
