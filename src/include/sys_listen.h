/* sys_listen -- Handlers listen syscall */

/* Copyright (c) 2010-2015. The SimGrid Team. All rights reserved.           */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU GPLv2) which comes with this package. */

#include "ptrace_utils.h"
#include "syscall_data.h"
#include "process_descriptor.h"

void syscall_listen(reg_s * reg, process_descriptor_t * proc);
void process_listen_call(reg_s * reg, process_descriptor_t * proc);
