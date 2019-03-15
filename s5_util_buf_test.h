#include "assert.h"
#include "s5_util_buf.h"

static void s5_buf_test() {
  int n = 4;
  s5_buf_t *p = s5_buf_create(n);

  assert(s5_buf_readable_bytes(p) == 0);
  assert(s5_buf_writable_bytes(p) == n);

  uint32_t val;

  // test 1

  s5_buf_write_int8(p, 1);
  s5_buf_write_int8(p, 2);
  s5_buf_write_int8(p, 3);
  s5_buf_write_int8(p, 4);
  s5_buf_print_readable_bytes(p);

  assert(s5_buf_readable_bytes(p) == 4);
  assert(s5_buf_writable_bytes(p) == n - 4);

  val = s5_buf_get_int16(p, 0);
  assert(val == (1 << 8 | 2));

  val = s5_buf_read_int32(p);
  assert(val == (1 << 24 | 2 << 16 | 3 << 8 | 4));

  assert(s5_buf_readable_bytes(p) == 0);
  assert(s5_buf_writable_bytes(p) == n);

  // test 2

  s5_buf_write_int16(p, 65535);
  s5_buf_print_readable_bytes(p);

  val = s5_buf_read_int16(p);
  assert(val == 65535);
}
