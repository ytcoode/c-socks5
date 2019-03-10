#ifndef _S5_UTIL_BUF_H
#define _S5_UTIL_BUF_H

#include <stdbool.h>

typedef struct {  // ring buffer
  char* buf;
  int cap;
  int ridx;
  int rbytes;
} s5_buf_t;

s5_buf_t* s5_buf_create(int cap);
void s5_buf_destroy(s5_buf_t* p);

int s5_buf_writable_bytes(s5_buf_t* p);
int s5_buf_readable_bytes(s5_buf_t* p);

int s5_buf_read(s5_buf_t* p, int fd);
int s5_buf_write(s5_buf_t* p, int fd);
int s5_buf_copy(s5_buf_t* p, int fd_in,
                int fd_out);  // -1: err, 0: close, 1: ok

bool s5_buf_get_byte(s5_buf_t* p, int* val, int idx);
bool s5_buf_get_short(s5_buf_t* p, int* val, int idx);
bool s5_buf_get_int(s5_buf_t* p, int* val, int idx);
bool s5_buf_get_bytes(s5_buf_t* p, char* buf, int len, int idx);

bool s5_buf_read_byte(s5_buf_t* p, int* val);
bool s5_buf_read_short(s5_buf_t* p, int* val);
bool s5_buf_read_int(s5_buf_t* p, int* val);
bool s5_buf_read_bytes(s5_buf_t* p, char* buf, int len);

bool s5_buf_write_byte(s5_buf_t* p, int val);
bool s5_buf_write_short(s5_buf_t* p, int val);
bool s5_buf_write_int(s5_buf_t* p, int val);

void s5_buf_print_readable_bytes(s5_buf_t* p);

#endif
