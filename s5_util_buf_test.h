#include "assert.h"
#include "s5_util_buf.h"

static void s5_buf_test() {
  int n = 4;
  s5_buf_t *p = s5_buf_create(n);

  assert(s5_buf_readable_bytes(p) == 0);
  assert(s5_buf_writable_bytes(p) == n);

  bool b;
  int val;

  // test 1

  b = s5_buf_write_byte(p, 1);
  assert(b);

  b = s5_buf_write_byte(p, 2);
  assert(b);

  b = s5_buf_write_byte(p, 3);
  assert(b);

  b = s5_buf_write_byte(p, 4);
  assert(b);

  s5_buf_print_readable_bytes(p);

  assert(s5_buf_readable_bytes(p) == 4);
  assert(s5_buf_writable_bytes(p) == n - 4);

  b = s5_buf_get_short(p, &val, 0);
  assert(b);
  assert(val == (1 << 8 | 2));

  b = s5_buf_read_int(p, &val);
  assert(b);
  assert(val == (1 << 24 | 2 << 16 | 3 << 8 | 4));

  assert(s5_buf_readable_bytes(p) == 0);
  assert(s5_buf_writable_bytes(p) == n);

  // test 2

  b = s5_buf_write_short(p, 65535);
  assert(b);

  s5_buf_print_readable_bytes(p);

  b = s5_buf_read_short(p, &val);
  assert(b);

  assert(val == 65535);
}
