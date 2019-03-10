#include "s5_util_buf.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>
#include "s5_util_misc.h"
#include "unistd.h"

static int s5_buf_get_byte0(s5_buf_t* p, int idx) {
  idx += p->ridx;
  if (idx >= p->cap) {
    idx -= p->cap;
  }
  return p->buf[idx] & 0xff;
}

static void s5_buf_write_byte0(s5_buf_t* p, int val) {
  int widx = p->ridx + p->rbytes;
  if (widx >= p->cap) {
    widx -= p->cap;
  }
  p->buf[widx] = val & 0xff;
  p->rbytes += 1;
}

static void s5_buf_inc_ridx(s5_buf_t* p, int n) {
  p->ridx += n;
  if (p->ridx >= p->cap) {
    p->ridx -= p->cap;
  }
  p->rbytes -= n;
}

s5_buf_t* s5_buf_create(int cap) {
  s5_buf_t* p = malloc(sizeof(s5_buf_t));
  if (p == NULL) {
    return NULL;
  }

  p->buf = malloc(cap);
  if (p->buf == NULL) {
    free(p);
    return NULL;
  }

  p->cap = cap;
  p->ridx = 0;
  p->rbytes = 0;
  return p;
}

void s5_buf_destroy(s5_buf_t* p) {
  free(p->buf);
  p->buf = NULL;
  free(p);
}

int s5_buf_writable_bytes(s5_buf_t* p) { return p->cap - p->rbytes; }

int s5_buf_readable_bytes(s5_buf_t* p) { return p->rbytes; }

int s5_buf_read(s5_buf_t* p, int fd) {
  assert(s5_buf_writable_bytes(p) > 0);
  struct iovec iov[2];
  int iovcnt;

  int widx = p->ridx + p->rbytes;
  if (widx < p->cap) {
    iov[0].iov_base = p->buf + widx;
    iov[0].iov_len = p->cap - widx;
    if (p->ridx > 0) {
      iov[1].iov_base = p->buf;
      iov[1].iov_len = p->ridx;
      iovcnt = 2;
    } else {
      iovcnt = 1;
    }
  } else {
    widx -= p->cap;
    iov[0].iov_base = p->buf + widx;
    iov[0].iov_len = p->ridx - widx;
    iovcnt = 1;
  }

  int n = readv(fd, iov, iovcnt);
  if (n != -1) {
    p->rbytes += n;
  }
  return n;
}

int s5_buf_write(s5_buf_t* p, int fd) {
  int len = s5_buf_readable_bytes(p);
  if (len == 0) {
    return 0;
  }
  assert(len > 0);

  struct iovec iov[2];
  int iovcnt;

  int widx = p->ridx + p->rbytes;
  if (widx <= p->cap) {
    iov[0].iov_base = p->buf + p->ridx;
    iov[0].iov_len = p->rbytes;
    iovcnt = 1;
  } else {
    iov[0].iov_base = p->buf + p->ridx;
    iov[0].iov_len = p->cap - p->ridx;
    iov[1].iov_base = p->buf;
    iov[1].iov_len = widx - p->cap;
    iovcnt = 2;
  }

  int n = writev(fd, iov, iovcnt);
  if (n != -1) {
    s5_buf_inc_ridx(p, n);
  }
  return n;
}

int s5_buf_copy(s5_buf_t* p, int fd_in, int fd_out) {
  int n;
  for (;;) {
  write:
    n = s5_buf_write(p, fd_out);
    if (n == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return 1;
      }
      if (errno == EINTR) {
        goto write;
      }
      perror("s5_buf_write");
      return -1;
    }

    if (s5_buf_readable_bytes(p) > 0) {
      goto write;
    }

  read:
    n = s5_buf_read(p, fd_in);
    if (n == 0) {
      return 0;
    }

    if (n == -1) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
        return 1;
      }
      if (errno == EINTR) {
        goto read;
      }
      perror("s5_buf_read");
      return -1;
    }
  }
}

bool s5_buf_get_byte(s5_buf_t* p, int* val, int idx) {
  if (s5_buf_readable_bytes(p) < idx + 1) {
    return false;
  }
  *val = s5_buf_get_byte0(p, idx);
  return true;
}

bool s5_buf_get_short(s5_buf_t* p, int* val, int idx) {
  if (s5_buf_readable_bytes(p) < idx + 2) {
    return false;
  }
  *val = s5_buf_get_byte0(p, idx) << 8 | s5_buf_get_byte0(p, idx + 1);
  return true;
}

bool s5_buf_get_int(s5_buf_t* p, int* val, int idx) {
  if (s5_buf_readable_bytes(p) < idx + 4) {
    return false;
  }

  *val = s5_buf_get_byte0(p, idx) << 24 | s5_buf_get_byte0(p, idx + 1) << 16 |
         s5_buf_get_byte0(p, idx + 2) << 8 | s5_buf_get_byte0(p, idx + 3);
  return true;
}

bool s5_buf_get_bytes(s5_buf_t* p, char* buf, int len, int idx) {
  if (s5_buf_readable_bytes(p) < idx + len) {
    return false;
  }
  for (int i = 0; i < len; i++) {
    buf[i] = s5_buf_get_byte0(p, idx + i);
  }
  return true;
}

bool s5_buf_read_byte(s5_buf_t* p, int* val) {  // TODO signed/unsigned
  bool b = s5_buf_get_byte(p, val, 0);
  if (b) {
    s5_buf_inc_ridx(p, 1);
  }
  return b;
}

bool s5_buf_read_short(s5_buf_t* p, int* val) {
  bool b = s5_buf_get_short(p, val, 0);
  if (b) {
    s5_buf_inc_ridx(p, 2);
  }
  return b;
}

bool s5_buf_read_int(s5_buf_t* p, int* val) {
  bool b = s5_buf_get_int(p, val, 0);
  if (b) {
    s5_buf_inc_ridx(p, 4);
  }
  return b;
}

bool s5_buf_read_bytes(s5_buf_t* p, char* buf, int len) {
  bool b = s5_buf_get_bytes(p, buf, len, 0);
  if (b) {
    s5_buf_inc_ridx(p, len);
  }
  return b;
}

bool s5_buf_write_byte(s5_buf_t* p, int val) {
  if (s5_buf_writable_bytes(p) < 1) {
    return false;
  }
  s5_buf_write_byte0(p, val);
  return true;
}

bool s5_buf_write_short(s5_buf_t* p, int val) {
  if (s5_buf_writable_bytes(p) < 2) {
    return false;
  }
  s5_buf_write_byte0(p, val >> 8 & 0xff);
  s5_buf_write_byte0(p, val);
  return true;
}

bool s5_buf_write_int(s5_buf_t* p, int val) {
  if (s5_buf_writable_bytes(p) < 4) {
    return false;
  }
  s5_buf_write_byte0(p, val >> 24 & 0xff);
  s5_buf_write_byte0(p, val >> 16 & 0xff);
  s5_buf_write_byte0(p, val >> 8 & 0xff);
  s5_buf_write_byte0(p, val);
  return true;
}

void s5_buf_print_readable_bytes(s5_buf_t* p) {
  for (int i = 0; i < p->rbytes; i++) {
    int idx = p->ridx + i;
    if (idx >= p->cap) {
      idx -= p->cap;
    }
    printf("s5_buf_byte: %d -> %d\n", idx, s5_buf_get_byte0(p, i));
  }
}
