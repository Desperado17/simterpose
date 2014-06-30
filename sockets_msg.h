#ifndef __SOCKETS_MSG_H
#define __SOCKETS_MSG_H

/*Declaration of state for socket*/
#define SOCKET_READ_OK  0x0001
#define SOCKET_CLOSED   0x0002
#define SOCKET_WR_NBLK  0x0004
#define SOCKET_SHUT     0x0008

/*Decalration of all typedef of structure declared below*/
typedef struct recv_information recv_information;
typedef struct process_info process_info;
struct infos_socket;

#define SOCK_OPT_REUSEADDR      0x0001


#include "sysdep.h"
#include "xbt.h"
#include "xbt/fifo.h"
#include "syscall_data_msg.h"
#include "communication_msg.h"
#include "process_descriptor_msg.h"


struct recv_information {
  xbt_fifo_t data_fifo;
  xbt_fifo_t recv_task;
  int quantity_recv;
};

struct infos_socket {
  fd_descriptor_t fd;
  comm_t comm;                  //point to the communication which socket involved in
  msg_host_t host;
  int domain;
  int protocol;
  unsigned int ip_local;
  int port_local;
  int flags;
  int option;
  int binded;
};

void init_socket_gestion(void);

void socket_exit(void);

recv_information *recv_information_new(void);

void register_accepting_socket(struct infos_socket *is, process_descriptor_t * proc, int sockfd);

void delete_socket(struct infos_socket *is);

void recv_information_destroy(recv_information * recv);

int handle_new_receive(process_descriptor_t * proc, syscall_arg_u * sysarg);

void handle_new_send(struct infos_socket *is, syscall_arg_u * sysarg);

int close_all_communication(process_descriptor_t * proc);

struct infos_socket *register_socket(process_descriptor_t * proc, int sockfd, int domain, int protocol);

void update_socket(process_descriptor_t * proc, int fd);

int socket_registered(process_descriptor_t * proc, int fd);

struct infos_socket *get_infos_socket(process_descriptor_t * proc, int fd);

void get_localaddr_port_socket(process_descriptor_t * proc, int fd);

void set_localaddr_port_socket(process_descriptor_t * proc, int fd, char *ip, int port);

int get_protocol_socket(process_descriptor_t * proc, int fd);

int get_domain_socket(process_descriptor_t * proc, int fd);

int socket_incomplete(process_descriptor_t * proc, int fd);

int socket_netlink(process_descriptor_t * proc, int fd);

int socket_get_remote_addr(process_descriptor_t * proc, int fd, struct sockaddr_in *addr_port);

struct infos_socket *getSocketInfoFromContext(unsigned int locale_ip, int locale_port);

int socket_get_state(struct infos_socket *is);

int socket_read_event(process_descriptor_t * proc, int fd);

void socket_close(process_descriptor_t * proc, int fd);

int socket_network(process_descriptor_t * proc, int fd);

void socket_set_flags(process_descriptor_t * proc, int fd, int flags);

int socket_get_flags(process_descriptor_t * proc, int fd);

void socket_set_option(process_descriptor_t * proc, int fd, int option, int value);

int socket_get_option(process_descriptor_t * proc, int fd, int option);

void socket_set_bind(process_descriptor_t * proc, int fd, int val);

int socket_is_binded(process_descriptor_t * proc, int fd);

int socket_get_local_port(process_descriptor_t * proc, int fd);

#endif
