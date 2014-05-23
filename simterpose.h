/* simterpose.h -- main configurations of simterpose                       */

/* Copyright (c) 2010-2014. The SimGrid Team. All rights reserved.         */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU GPL) which comes with this package. */

#ifndef SIMTERPOSE_H
#define SIMTERPOSE_H

//#define address_translation

#include <sys/types.h>

#define MAX_FD 1024
#define MAX_PID 32768

#define PORT_BIND       0x1
#define PORT_LOCAL      0x2
#define PORT_REMOTE     0x4

typedef struct time_desc time_desc;
typedef struct simterpose_station simterpose_station;
typedef struct port_desc port_desc;
typedef struct translate_desc translate_desc;

#include "process_descriptor.h"
#include "sockets.h"
#include "xbt.h"

//For num syscall see 
//file:///usr/share/gdb/syscalls/amd64-linux.xml


int nb_peek;
int nb_poke;
int nb_getregs;
int nb_setregs;
int nb_syscall;
int nb_traceme;
int nb_setoptions;
int nb_detach;
int nb_geteventmsg;



struct time_desc {
  pid_t pid;
  double start_time;
};

struct port_desc {
  int port_num;
  int real_port;
  int option;
  int amount_socket;
  struct infos_socket *bind_socket;
};

struct translate_desc {
  int port_num;
  unsigned int ip;
};

struct simterpose_station {
  unsigned int ip;
  xbt_dict_t port;
};

void add_to_sched_list(pid_t pid);

#endif