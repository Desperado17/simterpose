/* time.c - Program to test that functions about time are well intercepted */

/* Copyright (c) 2010-2015. The SimGrid Team. All rights reserved.            */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPLv2) which comes with this package. */

#include <stdlib.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/timeb.h>
#include <time.h>

int main(){

  struct timeb * tp = (struct timeb *) malloc (sizeof(struct timeb));
  ftime(tp);
  printf("tp->time = %ld\n", tp->time);
  printf("tp->time = %d\n", tp->millitm);
  
  time_t t = time(NULL);
  printf("time %ld\n", t);

  const time_t *timep = NULL;
  localtime(timep);
  
  mktime(NULL);
  
  struct timeval * tval = (struct timeval *) malloc(sizeof(struct timeval));
  __timezone_ptr_t tz = 0;
  gettimeofday(tval, tz);

  clock_getres(0, NULL);

  struct timespec *tl = (struct timespec *) malloc(sizeof(struct timespec));
  clock_gettime(0, tl);

  clock_settime(0, NULL);
    
  return 0;
}
