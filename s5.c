#include <stdlib.h>
#include <unistd.h>
#include "s5_dns_resolver.h"
#include "s5_server.h"
#include "s5_signal.h"
#include "s5_util.h"

static int parse_args(int argc, char** argv, int* port) {
  if (argc != 2) {
    return -1;
  }
  *port = atoi(argv[1]);
  return 0;
}

// https://tools.ietf.org/rfc/rfc1928
int main(int argc, char** argv) {
  int err, port;

  err = parse_args(argc, argv, &port);
  if (err) {
    printf("Usage: %s <port>\n", *argv);
    return -1;
  }

  s5_signal_init();

  s5_epoll_t* ep = s5_epoll_create();
  assert(ep);

  s5_server_t* s = s5_server_create(port, ep);
  assert(s);

  err = s5_dns_resolver_start(s, ep);
  assert(!err);

  err = s5_server_start(s, ep);
  if (err) {
    perror_and_exit("s5_server_start");
  }

  s5_epoll_run(ep);

  // TODO destory other?
  s5_epoll_destroy(ep);
  return 0;
}
