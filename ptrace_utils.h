#ifndef __PEEK_DATA_H 
#define __PEEK_DATA_H

#include <sys/types.h>

typedef struct{
  unsigned long reg_orig;
  unsigned long ret;
  unsigned long arg1;
  unsigned long arg2;
  unsigned long arg3;
  unsigned long arg4;
  unsigned long arg5;
}reg_s;


void ptrace_cpy(pid_t child, void * dst, void * src, size_t len, char *syscall);

void ptrace_poke(pid_t pid, void* dst, void* src, size_t len);

void ptrace_resume_process(const pid_t pid);

void ptrace_detach_process(const pid_t pid);

void ptrace_get_register(const pid_t pid, reg_s* arg);

unsigned long ptrace_get_pid_fork(const pid_t pid);

void ptrace_set_register(const pid_t pid);

void ptrace_rewind_syscalls(const pid_t pid);

void ptrace_neutralize_syscall(const pid_t pid);

void ptrace_restore_syscall(pid_t pid, unsigned long syscall, unsigned long result);

#endif

