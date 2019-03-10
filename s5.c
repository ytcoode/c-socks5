#include <stdlib.h>
#include "s5_server.h"
#include "s5_util.h"

// https://tools.ietf.org/rfc/rfc1928
int main(int argc, char** argv) {
  int err, port;

  if (argc != 2) {
    printf("Usage: ./s5 <port>\n");
    return -1;
  }
  port = atoi(argv[1]);

  err = signal_ignore(SIGPIPE);
  if (err) {
    perror_and_exit("signal_ignore");
  }

  s5_epoll_t* ep = s5_epoll_create();
  assert(ep);

  s5_server_t* s = s5_server_create(port);
  assert(s);

  err = s5_server_listen(s);
  if (err) {
    perror_and_exit("s5_server_listen");
  }

  err = s5_epoll_add(ep, s->fd, s);
  if (err) {
    perror_and_exit("s5_epoll_add");
  }

  s5_epoll_run(ep);
  s5_epoll_destroy(ep);

  return 0;
}
