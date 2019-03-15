#include "s5_dns_resolver.h"
#include <limits.h>
#include <pthread.h>
#include <unistd.h>
#include "s5_conn.h"
#include "s5_conn_negotiate.h"
#include "s5_server.h"

#define DN2IP_REQ_MAX_BN (sizeof(uintptr_t) + 1 + 255)

struct s5_dn2ip_fd_in {
  int fd;
  s5_buf_t *buf;
  uintptr_t dn2ip_req_l;
};

struct s5_dn2ip_fd_out {
  int fd;
  s5_buf_t *buf;
};

static s5_epoll_item_hdlr fd_in_hdlrs[] = {
    NULL,
    h_dn2ip_write_req,
};

static s5_epoll_item_hdlr fd_out_hdlrs[] = {
    h_dn2ip_read_reply,
    NULL,
};

static s5_dn2ip_fd_in_t *s5_dn2ip_fd_in_create(int fd, s5_epoll_t *ep) {
  if (s5_fcntl_set_nonblock(fd)) {
    perror("s5_fcntl_set_nonblock");
    return NULL;
  }

  s5_dn2ip_fd_in_t *p =
      s5_epoll_item_create(sizeof(s5_dn2ip_fd_in_t), fd_in_hdlrs, 0, ep);
  if (!p) {
    return NULL;
  }

  p->fd = fd;

  assert(DN2IP_REQ_MAX_BN <= PIPE_BUF);  // 保证原子写
  p->buf = s5_buf_create(DN2IP_REQ_MAX_BN);
  if (!p->buf) {
    s5_epoll_item_destroy(p, ep);
    return NULL;
  }

  p->dn2ip_req_l = 0;
  return p;
}

void s5_dn2ip_fd_in_destroy(void *p, s5_epoll_t *ep) {  // TODO
  s5_dn2ip_fd_in_t *f = p;

  if (f->fd != -1) {
    s5_close(f->fd);
    f->fd = -1;
  }
  if (f->buf) {
    s5_buf_destroy(f->buf);
    f->buf = NULL;
  }
  s5_epoll_item_destroy(p, ep);
}

static s5_dn2ip_fd_out_t *s5_dn2ip_fd_out_create(int fd, s5_epoll_t *ep) {
  if (s5_fcntl_set_nonblock(fd)) {
    perror("s5_fcntl_set_nonblock");
    return NULL;
  }

  s5_dn2ip_fd_out_t *p =
      s5_epoll_item_create(sizeof(s5_dn2ip_fd_out_t), fd_out_hdlrs, 0, ep);
  if (!p) {
    return NULL;
  }

  p->fd = fd;
  p->buf = s5_buf_create(1024);
  if (!p->buf) {
    s5_epoll_item_destroy(p, ep);
    return NULL;
  }
  return p;
}

void s5_dn2ip_fd_out_destroy(void *p, s5_epoll_t *ep) {  // TODO
  s5_dn2ip_fd_out_t *f = p;

  if (f->fd != -1) {
    s5_close(f->fd);
    f->fd = -1;
  }
  if (f->buf) {
    s5_buf_destroy(f->buf);
    f->buf = NULL;
  }
  s5_epoll_item_destroy(p, ep);
}

static void *s5_dns_resolver_thread_entry(void *p) {
  int *fds = p;
  int fd_in = fds[0];
  int fd_out = fds[1];
  s5_buf_t *buf_in = s5_buf_create(1024);
  s5_buf_t *buf_out = s5_buf_create(1024);  // must <= PIPE_BUF

  int n, err, rbytes;
  uintptr_t addr;
  uint32_t ip;
  char domain[256];
  pthread_t tid = pthread_self();

  S5_DEBUG("s5_dns_resolver_thread started: %ld\n", tid);

read:
  n = s5_buf_read(buf_in, fd_in);
  if (n == -1) {
    if (errno == EINTR) {
      goto read;
    }
    perror_and_exit("s5_buf_read");
  }
  assert(n > 0);

  for (;;) {
    rbytes = s5_buf_readable_bytes(buf_in);
    if (rbytes < sizeof(uintptr_t) + 1) {
      goto read;
    }

    n = s5_buf_get_int8(buf_in, sizeof(uintptr_t));
    if (rbytes < sizeof(uintptr_t) + 1 + n) {
      goto read;
    }

    addr = s5_buf_read_intptr(buf_in);
    s5_buf_skip_bytes(buf_in, 1);

    s5_buf_read_bytes(buf_in, domain, n);
    domain[n] = 0;

    // get ip
    err = s5_get_ip_by_domain(domain, &ip);

    // output
    s5_buf_write_intptr(buf_out, addr);
    s5_buf_write_int8(buf_out, err ? 0 : 1);
    if (!err) {
      s5_buf_write_int32(buf_out, ip);

      int ipb1 = (ip >> 24) & 0xff;  // TODO
      int ipb2 = (ip >> 16) & 0xff;
      int ipb3 = (ip >> 8) & 0xff;
      int ipb4 = (ip >> 0) & 0xff;

      S5_DEBUG("dn2ip %ld: %s -> %d.%d.%d.%d\n", tid, domain, ipb1, ipb2, ipb3,
               ipb4);
    }

  write:
    n = s5_buf_write(buf_out, fd_out);
    if (n == -1) {
      if (errno == EINTR) {
        goto write;
      }
      perror_and_exit("s5_buf_read");
    }

    if (s5_buf_readable_bytes(buf_out) > 0) {
      goto write;
    }
  }
  return NULL;
}

static void s5_dns_resolver_thread_start(int fd_in, int fd_out) {
  pthread_t tid;
  int err;

  int *arg = malloc(sizeof(int) * 2);
  arg[0] = fd_in;
  arg[1] = fd_out;

  err = pthread_create(&tid, NULL, s5_dns_resolver_thread_entry, arg);
  if (err) {
    perror_and_exit2(err, "pthread_create");
  }
}

int s5_dns_resolver_start(s5_server_t *s, s5_epoll_t *ep) {
  int pipefd_req[2];
  int pipefd_reply[2];
  int nthreads = 5;
  int i, err;

  // reply
  err = pipe(pipefd_reply);
  if (err) {
    perror_and_exit("pipe");
  }

  assert(!s->dn2ip_fd_out);
  s->dn2ip_fd_out = s5_dn2ip_fd_out_create(pipefd_reply[0], ep);
  assert(s->dn2ip_fd_out);

  err = s5_epoll_add(ep, s->dn2ip_fd_out->fd, s->dn2ip_fd_out);
  if (err) {
    perror_and_exit("s5_epoll_add");
  }

  // req
  assert(!s->dn2ip_fd_in);
  s->dn2ip_fd_in_n = nthreads;
  s->dn2ip_fd_in_i = 0;
  s->dn2ip_fd_in = malloc(sizeof(s5_dn2ip_fd_in_t *) * nthreads);
  assert(s->dn2ip_fd_in);

  for (i = 0; i < nthreads; i++) {
    err = pipe(pipefd_req);
    if (err) {
      perror_and_exit("pipe");
    }
    s->dn2ip_fd_in[i] = s5_dn2ip_fd_in_create(pipefd_req[1], ep);
    s5_dns_resolver_thread_start(pipefd_req[0], pipefd_reply[1]);

    err = s5_epoll_add(ep, s->dn2ip_fd_in[i]->fd, s->dn2ip_fd_in[i]);
    if (err) {
      perror_and_exit("s5_epoll_add");
    }
  }

  return 0;
}

int s5_dn2ip_write_req_or_link(s5_server_t *s, s5_conn_t *c) {
  s5_dn2ip_fd_in_t *fin;
  int n;

  fin = s->dn2ip_fd_in[s->dn2ip_fd_in_i++];
  if (s->dn2ip_fd_in_i >= s->dn2ip_fd_in_n) {
    s->dn2ip_fd_in_i = 0;
  }

  if (!fin->dn2ip_req_l) {
    n = s5_dn2ip_write_req(fin, c);
    if (n == -1) {
      return -1;
    }
    if (n == 1) {
      return 0;
    }
  }

  assert(!c->dn2ip_req_l);
  c->dn2ip_req_l = fin->dn2ip_req_l;
  fin->dn2ip_req_l = (uintptr_t)&c;
  return 0;
}

int s5_dn2ip_write_req(s5_dn2ip_fd_in_t *fin, s5_conn_t *c) {
  int n, nb1, nb2;
  char *buf;

  s5_buf_empty(fin->buf);
  s5_buf_write_intptr(fin->buf, (uintptr_t)c);  // ptr

  nb1 = s5_buf_get_int8(c->buf, 0);
  s5_buf_get_buf(c->buf, fin->buf, ++nb1, 0);  // len + domain name

  buf = s5_buf_raw(fin->buf);
  nb2 = s5_buf_readable_bytes(fin->buf);

write:
  n = write(fin->fd, buf, nb2);
  if (n == -1) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return 0;
    }
    if (errno == EINTR) {
      goto write;
    }
    perror("write");
    return -1;
  }

  assert(n == nb2);  // pipe 是原子写
  s5_buf_skip_bytes(c->buf, nb1);
  return 1;
}

void h_dn2ip_write_req(void *p, s5_epoll_t *ep) {
  s5_dn2ip_fd_in_t *fin = p;
  s5_conn_t *c;
  int n;

  while (fin->dn2ip_req_l) {
    c = (s5_conn_t *)fin->dn2ip_req_l;
    n = s5_dn2ip_write_req(fin, c);
    assert(n != -1);
    if (n == 0) {
      break;
    }
    assert(n > 0);
    fin->dn2ip_req_l = c->dn2ip_req_l;
    c->dn2ip_req_l = 0;
  }
}

void h_dn2ip_read_reply(void *p, s5_epoll_t *ep) {
  s5_dn2ip_fd_out_t *fout = p;
  s5_conn_t *c;
  uintptr_t addr;
  uint32_t ip;
  int n, ok, rbytes;

read:
  n = s5_buf_read(fout->buf, fout->fd);
  assert(n);

  if (n == -1) {
    if (errno == EAGAIN || errno == EWOULDBLOCK) {
      return;
    }
    if (errno == EINTR) {
      goto read;
    }
    perror_and_exit("s5_buf_read");
  }
  assert(n > 0);

restart:
  // ptr + ok + ipv4
  rbytes = s5_buf_readable_bytes(fout->buf);
  if (rbytes < sizeof(uintptr_t) + 1) {
    goto read;
  }

  ok = s5_buf_get_int8(fout->buf, sizeof(uintptr_t));
  if (ok) {
    if (rbytes < sizeof(uintptr_t) + 1 + 4) {
      goto read;
    }
  }

  addr = s5_buf_read_intptr(fout->buf);
  c = (s5_conn_t *)addr;

  s5_buf_skip_bytes(fout->buf, 1);  // ok

  if (!ok) {
    goto close;
  }

  ip = s5_buf_read_int32(fout->buf);
  s5_connect_target(c, ip, ep);
  goto restart;

close:
  s5_conn_destroy(c, ep);
}
