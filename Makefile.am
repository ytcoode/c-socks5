
AM_CFLAGS = -pedantic -Wall -Werror # -std=c11 # TODO

SHARED_SRCS = \
  s5_server.c \
  s5_server.h \
  s5_conn.c \
  s5_conn.h \
  s5_conn_negotiate.c \
  s5_conn_negotiate.h \
  s5_conn_relay.c \
  s5_conn_relay.h \
  s5_conn_shutdown.c \
  s5_conn_shutdown.h \
  s5_util.h \
  s5_util_epoll.c \
  s5_util_epoll.h \
  s5_util_sock.c \
  s5_util_sock.h \
  s5_util_buf.c \
  s5_util_buf.h \
  s5_util_misc.c \
  s5_util_misc.h \
  s5_util_log.h

bin_PROGRAMS = s5 s5_test

s5_SOURCES = \
  s5.c \
  $(SHARED_SRCS)

s5_test_SOURCES = \
  s5_test.c \
  s5_util_buf_test.h \
  $(SHARED_SRCS)

reformat:
	clang-format -style=Google -i $(s5_SOURCES) $(s5_test_SOURCES) $(SHARED_SRCS)
