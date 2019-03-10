#ifndef _S5_UTIL_LOG_H
#define _S5_UTIL_LOG_H

#include <stdio.h>
// TODO better impl

#define S5_LOG_LEVEL 1

#define S5_LOG(level, ...)                          \
  do {                                              \
    if (level >= S5_LOG_LEVEL) printf(__VA_ARGS__); \
  } while (0)

#define S5_DEBUG(...) S5_LOG(1, __VA_ARGS__)
#define S5_INFO(...) S5_LOG(2, __VA_ARGS__)
#define S5_ERROR(...) S5_LOG(3, __VA_ARGS__)

#endif
