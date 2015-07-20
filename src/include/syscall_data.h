/* syscall_data -- Structures to store syscall arguments */

/* Copyright (c) 2010-2015. The SimGrid Team. All rights reserved.         */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU GPLv2) which comes with this package. */

#ifndef SYSCALL_DATA_H
#define SYSCALL_DATA_H

/* Q. Where does this data come from?
 * A. From the linux source code itself. See for example:
 *    http://blog.rchapman.org/post/36801038863/linux-system-call-table-for-x86-64
 *
 * Excerpt of that page:
 *
 * To find the implementation of a system call, grep the kernel tree for SYSCALL_DEFINE.\?(syscall,
 * For example, to find the read system call:
 *
 *   illusion:/usr/src/linux-source-3.2.0$ grep -rA3 'SYSCALL_DEFINE.\?(read,' *
 *   fs/read_write.c:SYSCALL_DEFINE3(read, unsigned int, fd, char __user *, buf, size_t, count)
 *   fs/read_write.c-{
 *   fs/read_write.c-        struct file *file;
 *   fs/read_write.c-        ssize_t ret = -EBADF;
 *
 * The results show that the implementation is in fs/read_write.c and that it takes 3 arguments (thus SYSCALL_DEFINE3).
 *
 */

#include "sysdep.h"

#define SELECT_FDRD_SET 0x01
#define SELECT_FDWR_SET 0x02
#define SELECT_FDEX_SET 0x04

typedef struct recvmsg_arg_s {
  int sockfd;
  size_t len;
  void *data;
  int flags;
  struct msghdr msg;
  ssize_t ret;
} recvmsg_arg_s, sendmsg_arg_s;

typedef sendmsg_arg_s *sendmsg_arg_t;
typedef recvmsg_arg_s *recvmsg_arg_t;

typedef struct sendto_arg_s {
  int sockfd;
  void *data;
  size_t len;
  int flags;
  socklen_t addrlen;
  void *dest;                   //address in processus of data
  int is_addr;                  //indicate if struct sockadrr is null or not
 union {
    struct sockaddr_in sai;
    struct sockaddr_un sau;
    struct sockaddr_nl snl;
  };
  ssize_t ret;
} sendto_arg_s, recvfrom_arg_s;

typedef sendto_arg_s *sendto_arg_t;
typedef recvfrom_arg_s *recvfrom_arg_t;


typedef struct connect_bind_arg_s {
  int sockfd;
  union {
    struct sockaddr_in sai;
    struct sockaddr_un sau;
    struct sockaddr_nl snl;
  };
  socklen_t addrlen;
  int ret;
} connect_arg_s, bind_arg_s;

typedef connect_arg_s *connect_arg_t;
typedef bind_arg_s *bind_arg_t;


typedef struct accept_arg_s {
  int sockfd;
  union {
    struct sockaddr_in sai;
    struct sockaddr_un sau;
    struct sockaddr_nl snl;
  };
  socklen_t addrlen;
  void *addr_dest;
  void *len_dest;
  int ret;
} accept_arg_s;

typedef accept_arg_s *accept_arg_t;

typedef struct write_arg_s {
  int fd;
  void *data;
  size_t count;
  void *dest; /* TODO weird what is this?*/
  ssize_t ret;
} write_arg_s, read_arg_s;

typedef write_arg_s *write_arg_t;
typedef read_arg_s *read_arg_t;

typedef struct clone_arg_s { /* TODO missing argument*/
  unsigned long newsp;
  void *parent_tid;
  void *child_tid;
  int flags;
  int ret;
} clone_arg_s;
typedef clone_arg_s *clone_arg_t;

typedef union {
  connect_arg_s connect;
  bind_arg_s bind;
  accept_arg_s accept;
  sendto_arg_s sendto;
  recvfrom_arg_s recvfrom;
  recvmsg_arg_s recvmsg;
  sendmsg_arg_s sendmsg;
  read_arg_s read;
  write_arg_s write;
  clone_arg_s clone;
} syscall_arg_u;


#endif
