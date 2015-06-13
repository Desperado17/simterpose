/* process_descriptor */

/* Copyright (c) 2010-2015. The SimGrid Team. All rights reserved.         */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU GPLv2) which comes with this package. */

#ifndef __PROCESS_DESCRIPTOR_H
#define __PROCESS_DESCRIPTOR_H

#define PROC_RECVMSG            0x000200
#define PROC_READ               0x000400
#define PROC_RECVFROM           0x000800


#define PROC_NO_STATE           0x000000
#define PROC_SELECT             0x000010
#define PROC_POLL               0x000020
#define PROC_CONNECT            0x000040
#define PROC_ACCEPT		        0x000080
#define PROC_CONNECT_DONE       0x000100
#define PROC_RECV            (PROC_RECVMSG | PROC_RECVFROM | PROC_READ)

#define STATE_MASK              0x00FFF0

#define PROC_IDLE_IN_TASK       0x00100

#define PROC_NO_TIMEOUT         0
#define PROC_IN_TIMEOUT         1
#define PROC_TIMEOUT_EXPIRE     2


#define FD_STDIN                0x00
#define FD_STDOUT               0x01
#define FD_STDERR               0x02
#define FD_CLASSIC              0x04
#define FD_SOCKET               0x08
#define FD_PIPE                 0x10

#define MAX_FD 2048

#include <stdio.h>
#include <sys/types.h>

#include <simgrid/msg.h>
#include "syscall_data.h"

typedef struct process_descriptor process_descriptor_t;

typedef struct {
  msg_sem_t sem_client;
  msg_sem_t sem_server;
  msg_process_t client;
  msg_process_t server;
  const char *to_client;        // name of the mailbox
  const char *to_server;        // name of the mailbox
} stream_t;

typedef struct pipe_end_s pipe_end_s;
typedef pipe_end_s *pipe_end_t;

struct pipe_end_s {
  int fd;
  process_descriptor_t *proc;
};

typedef struct {
  xbt_dynar_t read_end;
  xbt_dynar_t write_end;
} pipe_t;

typedef struct {
  int type;
  process_descriptor_t *proc;
  int fd;
  stream_t *stream;
  pipe_t *pipe;
  int flags;
  int refcount;                   // reference counting
  int lock;
  off_t begin;
  off_t end;
  pid_t proc_locker;
} fd_descriptor_t;

struct process_descriptor {
  pid_t pid;
  char *name;
  msg_host_t host;
  fd_descriptor_t **fd_list;
  int status;

  int in_syscall:1; // whether we are inside or outside of the syscall

  syscall_arg_u sysarg;

  FILE* strace_out; // (real) file descriptor to use to write the strace-like output when ran in --strace mode
  int curcol;
};

#define getevent(status) (( (status) >> 16) & 0xffff)
static int proc_event_exec(process_descriptor_t *proc) {
  return WIFSTOPPED(proc->status) &&  ( getevent(proc->status) == PTRACE_EVENT_EXEC );
}
static int proc_event_syscall(process_descriptor_t *proc) {
  return WIFSTOPPED(proc->status) &&  ( WSTOPSIG(proc->status) & (SIGTRAP | 0x80) );
}

static int proc_entering(process_descriptor_t *proc) {
  if (! proc_event_syscall(proc) ) {
    unsigned int event = (proc->status >> 16) & 0xffff;
    switch (event) {
    case PTRACE_EVENT_FORK:
      fprintf(stderr, "[%d] That's not a syscall-stop event but a fork!\n", proc->pid);
      break;
    case PTRACE_EVENT_VFORK:
      fprintf(stderr, "[%d] That's not a syscall-stop event but a vfork!\n", proc->pid);
      break;
    case PTRACE_EVENT_CLONE:
      fprintf(stderr, "[%d] That's not a syscall-stop event but a clone!\n", proc->pid);
      break;
    case PTRACE_EVENT_VFORK_DONE:
      fprintf(stderr, "[%d] That's not a syscall-stop event but a fork_done!\n", proc->pid);
      break;
    case PTRACE_EVENT_EXEC:
      fprintf(stderr, "[%d] That's not a syscall-stop event but an exec!\n", proc->pid);
      break;
    case PTRACE_EVENT_EXIT:
      fprintf(stderr, "[%d] That's not a syscall-stop event but an exit!\n", proc->pid);
      break;
    default:
      fprintf(stderr, "[%d] That's not a syscall-stop event but I'm not sure which event that is  :-(\n", proc->pid);
      break;
    }
    xbt_backtrace_display_current();
  }
  return !proc->in_syscall;
}
static void proc_inside(process_descriptor_t *proc) {
  proc->in_syscall = 1;
}
static void proc_outside(process_descriptor_t *proc) {
  proc->in_syscall = 0;
}

process_descriptor_t *process_descriptor_new(const char *name, const char *argv0, pid_t pid);

void process_die(process_descriptor_t * proc);

#endif
