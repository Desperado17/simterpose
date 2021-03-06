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

#endif
