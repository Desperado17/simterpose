/* send_client -- A simple client talking to send_server using send/recv     */
/*                Its only merit is to constitute a test case for simterpose */

/* Copyright (c) 2010-2015. The SimGrid Team. All rights reserved.           */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU GPLv2) which comes with this package. */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <time.h>

int main(int argc, char **argv) {

  if (argc < 5) {
    fprintf(stderr, "Usage: %s IP port msg_count msg_size \n", argv[0]);
    return EXIT_FAILURE;
  }

  char* IP = argv[1];
  u_short server_port = atoi(argv[2]);
  int msg_count = atoi(argv[3]);
  int msg_size = atoi(argv[4]);

  fprintf(stderr, "Client starting: #msg: %d; size:%d (the server is on %s:%d) \n", msg_count, msg_size, IP, server_port);

  int clientSocket;
  int res;
  char *buff = malloc(msg_size);
  char *expected = malloc(msg_size);
  // long host_addr = inet_addr(IP);
  struct hostent *serverHostEnt;

  if ((clientSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    perror("Client: error while creating the real socket");
    exit(1);
  }
  struct sockaddr_in cli_addr;
  memset(&cli_addr, 0, sizeof(struct sockaddr_in));
  serverHostEnt = gethostbyname(IP);
  memcpy(&(cli_addr.sin_addr), serverHostEnt->h_addr, serverHostEnt->h_length);
  cli_addr.sin_family = AF_INET;
  cli_addr.sin_port = htons(server_port);

  if (connect(clientSocket, (struct sockaddr *) &cli_addr, sizeof(cli_addr)) < 0) {
    fprintf(stderr, "Client: Cannot connect to server: %s\n", strerror(errno));
    exit(1);
  }

  int msg_number = 0;

  for (msg_number = 0; msg_number < msg_count; ++msg_number) {
    fprintf(stderr, "Client: you will try to use recv/send syscalls, they are only suported on 32bits architectures. It could not work\n");
    sprintf(buff, "This is the message #%d produced on the client.", msg_number);
    res = send(clientSocket, buff, msg_size, 0);
    if (res == -1) {
      perror("Client: cannot send message");
      exit(1);
    }
    fprintf(stderr, "Client: sent message #%d\n", msg_number);

    int length = msg_size;
    while (length > 0) {
      res = recv(clientSocket, buff, length, 0);
      if (res == -1) {
        fprintf(stderr, "Client: Error while sending message #%d: %s\n", msg_number, strerror(errno));
        exit(1);
      }
      length -= res;
    }
    sprintf(expected, "This is the answer #%d, from the server.", msg_number);
    if (strcmp(buff, expected)) {
      fprintf(stderr, "Client: received answer does not match at step %d (got: %s)\n", msg_number, buff);
      exit(1);
    }
    fprintf(stderr, "Client: reception of answer #%d was successful\n", msg_number);
  }
  shutdown(clientSocket, 2);
  close(clientSocket);

  fprintf(stderr, "Client exiting after %d msgs \n", msg_count);

  return 0;
}
