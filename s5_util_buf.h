#ifndef _S5_UTIL_BUF_H
#define _S5_UTIL_BUF_H

#include <stdbool.h>
#include <stdint.h>

typedef struct s5_buf s5_buf_t;

s5_buf_t* s5_buf_create(int cap);
void s5_buf_destroy(s5_buf_t* p);
char* s5_buf_raw(s5_buf_t* p);

void s5_buf_empty(s5_buf_t* p);
bool s5_buf_is_empty(s5_buf_t* p);

int s5_buf_writable_bytes(s5_buf_t* p);
int s5_buf_readable_bytes(s5_buf_t* p);

void s5_buf_skip_bytes(s5_buf_t* p, int n);

int s5_buf_read(s5_buf_t* p, int fd);
int s5_buf_write(s5_buf_t* p, int fd);
int s5_buf_copy(s5_buf_t* p, int fd_in,
                int fd_out);  // -1: err, 0: close, 1: ok

uint8_t s5_buf_get_int8(s5_buf_t* p, int idx);
uint16_t s5_buf_get_int16(s5_buf_t* p, int idx);
uint32_t s5_buf_get_int32(s5_buf_t* p, int idx);
uint64_t s5_buf_get_int64(s5_buf_t* p, int idx);
uintptr_t s5_buf_get_intptr(s5_buf_t* p, int idx);
void s5_buf_get_bytes(s5_buf_t* p, char* buf, int len, int idx);
void s5_buf_get_buf(s5_buf_t* p, s5_buf_t* buf, int len, int idx);

uint8_t s5_buf_read_int8(s5_buf_t* p);
uint16_t s5_buf_read_int16(s5_buf_t* p);
uint32_t s5_buf_read_int32(s5_buf_t* p);
uint64_t s5_buf_read_int64(s5_buf_t* p);
uintptr_t s5_buf_read_intptr(s5_buf_t* p);
void s5_buf_read_bytes(s5_buf_t* p, char* buf, int len);
void s5_buf_read_buf(s5_buf_t* p, s5_buf_t* buf, int len);

void s5_buf_write_int8(s5_buf_t* p, uint8_t val);
void s5_buf_write_int16(s5_buf_t* p, uint16_t val);
void s5_buf_write_int32(s5_buf_t* p, uint32_t val);
void s5_buf_write_int64(s5_buf_t* p, uint64_t val);
void s5_buf_write_intptr(s5_buf_t* p, uintptr_t val);
void s5_buf_write_bytes(s5_buf_t* p, char* buf, int len);
void s5_buf_write_buf(s5_buf_t* p, s5_buf_t* buf, int len);

void s5_buf_print_readable_bytes(s5_buf_t* p);

#endif
