#include "s5_util_buf.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>
#include "s5_util_misc.h"
#include "unistd.h"

#define S5_BUF_GET_INT(p, idx, t)                                      \
  assert(s5_buf_readable_bytes(p) >= idx + sizeof(t));                 \
  t val = 0;                                                           \
  for (int i = 0; i < sizeof(t); i++)                                  \
    val |= (t)s5_buf_get_byte0(p, idx + i) << (sizeof(t) - i - 1) * 8; \
  return val;

#define S5_BUF_WRITE_INT(p, val, n)      \
  assert(s5_buf_writable_bytes(p) >= n); \
  for (int i = 1; i <= n; i++) s5_buf_write_byte0(p, val >> (n - i) * 8);

struct s5_buf {  // ring buffer
  char* buf;
  int cap;
  int ridx;
  int rbytes;
};

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

s5_buf_t* s5_buf_create(int cap) {
  assert(cap > 0);
  s5_buf_t* p = malloc(sizeof(s5_buf_t));
  if (!p) {
    return NULL;
  }

  p->buf = malloc(cap);
  if (!p->buf) {
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
  free(p);
}

char* s5_buf_raw(s5_buf_t* p) { return p->buf; }

void s5_buf_empty(s5_buf_t* p) {
  p->ridx = 0;
  p->rbytes = 0;
}

bool s5_buf_is_empty(s5_buf_t* p) { return p->rbytes == 0; }

int s5_buf_writable_bytes(s5_buf_t* p) { return p->cap - p->rbytes; }

int s5_buf_readable_bytes(s5_buf_t* p) { return p->rbytes; }

void s5_buf_skip_bytes(s5_buf_t* p, int n) {
  assert(p->rbytes >= n);
  p->ridx += n;
  if (p->ridx >= p->cap) {
    p->ridx -= p->cap;
  }
  p->rbytes -= n;
}

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
  if (n > 0) {
    p->rbytes += n;
  }
  return n;
}

int s5_buf_write(s5_buf_t* p, int fd) {
  if (s5_buf_is_empty(p)) {
    return 0;
  }

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
  if (n > 0) {
    s5_buf_skip_bytes(p, n);
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

uint8_t s5_buf_get_int8(s5_buf_t* p, int idx) {
  S5_BUF_GET_INT(p, idx, uint8_t);
}

uint16_t s5_buf_get_int16(s5_buf_t* p, int idx) {
  S5_BUF_GET_INT(p, idx, uint16_t);
}

uint32_t s5_buf_get_int32(s5_buf_t* p, int idx) {
  S5_BUF_GET_INT(p, idx, uint32_t);
}

uint64_t s5_buf_get_int64(s5_buf_t* p, int idx) {
  S5_BUF_GET_INT(p, idx, uint64_t);
}
uintptr_t s5_buf_get_intptr(s5_buf_t* p, int idx) {
  S5_BUF_GET_INT(p, idx, uintptr_t);
}

void s5_buf_get_bytes(s5_buf_t* p, char* buf, int len, int idx) {
  assert(s5_buf_readable_bytes(p) >= idx + len);
  for (int i = 0; i < len; i++)
    buf[i] = s5_buf_get_byte0(p, idx + i);  // TODO optimize
}

void s5_buf_get_buf(s5_buf_t* p, s5_buf_t* buf, int len, int idx) {
  assert(s5_buf_readable_bytes(p) >= idx + len);
  assert(s5_buf_writable_bytes(buf) >= len);
  for (int i = 0; i < len; i++)
    s5_buf_write_byte0(buf, s5_buf_get_byte0(p, idx + i));  // TODO optimize
}

uint8_t s5_buf_read_int8(s5_buf_t* p) {
  uint8_t val = s5_buf_get_int8(p, 0);
  s5_buf_skip_bytes(p, 1);
  return val;
}

uint16_t s5_buf_read_int16(s5_buf_t* p) {
  uint16_t val = s5_buf_get_int16(p, 0);
  s5_buf_skip_bytes(p, 2);
  return val;
}

uint32_t s5_buf_read_int32(s5_buf_t* p) {
  uint32_t val = s5_buf_get_int32(p, 0);
  s5_buf_skip_bytes(p, 4);
  return val;
}

uint64_t s5_buf_read_int64(s5_buf_t* p) {
  uint64_t val = s5_buf_get_int64(p, 0);
  s5_buf_skip_bytes(p, 8);
  return val;
}

uintptr_t s5_buf_read_intptr(s5_buf_t* p) {
  uintptr_t val = s5_buf_get_intptr(p, 0);
  s5_buf_skip_bytes(p, sizeof(uintptr_t));
  return val;
}

void s5_buf_read_bytes(s5_buf_t* p, char* buf, int len) {
  s5_buf_get_bytes(p, buf, len, 0);
  s5_buf_skip_bytes(p, len);
}

void s5_buf_read_buf(s5_buf_t* p, s5_buf_t* buf, int len) {
  s5_buf_get_buf(p, buf, len, 0);
  s5_buf_skip_bytes(p, len);
}

void s5_buf_write_int8(s5_buf_t* p, uint8_t val) {
  S5_BUF_WRITE_INT(p, val, 1);
}

void s5_buf_write_int16(s5_buf_t* p, uint16_t val) {
  S5_BUF_WRITE_INT(p, val, 2);
}

void s5_buf_write_int32(s5_buf_t* p, uint32_t val) {
  S5_BUF_WRITE_INT(p, val, 4);
}

void s5_buf_write_int64(s5_buf_t* p, uint64_t val) {
  S5_BUF_WRITE_INT(p, val, 8);
}

void s5_buf_write_intptr(s5_buf_t* p, uintptr_t val) {
  S5_BUF_WRITE_INT(p, val, sizeof(uintptr_t));
}

void s5_buf_write_bytes(s5_buf_t* p, char* buf, int len) {
  assert(s5_buf_writable_bytes(p) >= len);
  for (int i = 0; i < len; i++) s5_buf_write_byte0(p, buf[i]);  // TODO optimize
}

void s5_buf_write_buf(s5_buf_t* p, s5_buf_t* buf, int len) {
  s5_buf_read_buf(buf, p, len);
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
