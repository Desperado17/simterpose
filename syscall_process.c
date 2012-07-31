#include "syscall_process.h"
#include "syscall_data.h"
#include "insert_trace.h"
#include "sockets.h"
#include "run_trace.h"
#include "data_utils.h"
#include "ptrace_utils.h"
#include "process_descriptor.h"
#include "args_trace.h"
#include "task.h"
#include "xbt.h"
#include "simdag/simdag.h"
#include "xbt/log.h"
#include "communication.h"
#include "print_syscall.h"
#include "syscall_list.h"

#include <linux/futex.h>

XBT_LOG_NEW_DEFAULT_SUBCATEGORY(SYSCALL_PROCESS, SIMTERPOSE, "Syscall process log");

int process_accept_in_call(pid_t pid, syscall_arg_u* sysarg);
int process_recv_in_call(int pid, int fd);
void process_recvfrom_out_call(int pid);
void process_read_out_call(pid_t pid);
void process_recvmsg_out_call(pid_t pid);
void process_accept_out_call(pid_t pid, syscall_arg_u* sysarg);



//TODO test the possibility to remove incomplete checking
//There is no need to return value because send always bring a task
int process_send_call(int pid, syscall_arg_u* sysarg)
{
  send_arg_t arg = &(sysarg->send);
  if (socket_registered(pid,arg->sockfd) != -1) {
    if (!socket_netlink(pid,arg->sockfd))
    {
//       printf("%d This is not a netlink socket\n", arg->sockfd);
      calculate_computation_time(pid);
      struct infos_socket *is = get_infos_socket(pid,arg->sockfd);
      struct infos_socket *s = comm_get_peer(is);

//       printf("Sending data(%d) to %d on socket %d\n", arg->ret, s->proc->pid, s->fd);
      int peer_stat = process_get_state(s->fd.proc);
      if(peer_stat == PROC_SELECT || peer_stat == PROC_POLL || peer_stat == PROC_RECV_IN)
        add_to_sched_list(s->fd.proc->pid);
      
      handle_new_send(is,  sysarg);

      SD_task_t task = create_send_communication_task(pid, is, arg->ret);

      schedule_comm_task(is->fd.proc->station, s->fd.proc->station, task);
      is->fd.proc->on_simulation = 1;
      return 1;
    }
    return 0;
  }
  else 
    THROW_IMPOSSIBLE;
}

int process_recv_call(int pid, syscall_arg_u* sysarg)
{
  recv_arg_t arg = &(sysarg->recv);
  if (socket_registered(pid,arg->sockfd) != -1) {
    if (!socket_netlink(pid,arg->sockfd))
    {
      calculate_computation_time(pid);
      
      //if handle_new_receive return 1, there is a task found
      if(handle_new_receive(pid, sysarg))
        return PROCESS_TASK_FOUND;
      else
      {
        if(!socket_network(pid, arg->sockfd))
          return PROCESS_NO_TASK_FOUND;
        
        struct infos_socket* is = get_infos_socket(pid, arg->sockfd);
        int sock_status = socket_get_state(is);
        if(sock_status & SOCKET_CLOSED)
          return RECV_CLOSE;
        
        return PROCESS_NO_TASK_FOUND;
      }
    }
  }
  else
    THROW_IMPOSSIBLE;
  
  return 0;
}

int process_fork_call(int pid)
{
  THROW_UNIMPLEMENTED;
  return 1;
}


int process_select_call(pid_t pid)
{
//   printf("Entering select\n");
  process_descriptor *proc= process_get_descriptor(pid);
  select_arg_t arg = &(proc->sysarg.select);
  int i;
  
  fd_set fd_rd, fd_wr, fd_ex;
  
  fd_rd = arg->fd_read;
  fd_wr = arg->fd_write;
  fd_ex = arg->fd_except;
  
  int match = 0;
  
  for(i=0 ; i < arg->maxfd ; ++i)
  {
    struct infos_socket* is = get_infos_socket(pid, i);
    //if i is NULL that means that i is not a socket
    if(is == NULL)
    {
      FD_CLR(i, &(fd_rd));
      FD_CLR(i, &(fd_wr));
      continue;
    }

    int sock_status = socket_get_state(is);
    if(FD_ISSET(i, &(fd_rd)))
    {
      if(sock_status & SOCKET_READ_OK || sock_status & SOCKET_CLOSED || sock_status & SOCKET_SHUT)
        ++match;
      else
        FD_CLR(i, &(fd_rd));
    }
    if(FD_ISSET(i, &(fd_wr)))
    {
      if(sock_status & SOCKET_WR_NBLK && !(sock_status & SOCKET_CLOSED) && !(sock_status & SOCKET_SHUT))
        ++match;
      else
        FD_CLR(i, &(fd_wr));
    }
    if(FD_ISSET(i, &(fd_ex)))
    {
      XBT_WARN("Mediation for exception states on socket are not support yet\n");
    }
  }
  if(match > 0)
  {
//     printf("match for select\n");
    arg->fd_read = fd_rd;
    arg->fd_write = fd_wr;
    arg->fd_except = fd_ex;
    sys_build_select(pid, match);
//     print_select_syscall(pid, &(proc->sysarg));
    return match;
  }
  
  if(proc->in_timeout == PROC_TIMEOUT_EXPIRE)
  {
//     printf("Timeout for select\n");
    
    FD_ZERO(&fd_rd);
    FD_ZERO(&fd_wr);
    FD_ZERO(&fd_ex);
    arg->ret=0;
    arg->fd_read = fd_rd;
    arg->fd_write = fd_wr;
    arg->fd_except = fd_ex;
    sys_build_select(pid, 0);
//     print_select_syscall(pid, &(proc->sysarg));
    proc->in_timeout = PROC_NO_TIMEOUT;
    return 1;
  }
  return 0;
}


int process_poll_call(pid_t pid)
{
  process_descriptor *proc = process_get_descriptor(pid);
  
//   printf("Entering poll %lf %p\n", SD_get_clock(), proc->timeout);
  poll_arg_t arg = (poll_arg_t)&(proc->sysarg.poll);
  
  int match=0;
  int i;
  
  for(i=0; i < arg->nbfd; ++i)
  {
    struct pollfd *temp = &(arg->fd_list[i]);
    
    struct infos_socket *is = get_infos_socket(pid, temp->fd);
    if(is == NULL)
      continue;
    else
    {
      int sock_status = socket_get_state(is);
      if(temp->events & POLLIN)
      {
        if(sock_status & SOCKET_READ_OK || sock_status & SOCKET_CLOSED)
        {
          temp->revents = temp->revents | POLLIN;
          ++match;
        }
        else
        {
          temp->revents = temp->revents & ~POLLIN;
        }
      }
      else if(temp->events & POLLOUT)
      {
        if(sock_status & SOCKET_WR_NBLK)
        {
          temp->revents = temp->revents | POLLOUT;
          ++match;
        }
        else
        {
          temp->revents = temp->revents & ~POLLOUT;
        }
      }
      else
        XBT_WARN("Mediation different than POLLIN are not handle for poll\n");
    }
  }
  if(match > 0)
  {
//     printf("Result for poll\n");
    sys_build_poll(pid, match);
//     print_poll_syscall(pid, &(proc->sysarg));
    free(proc->sysarg.poll.fd_list);
    return match;
  }
  if(proc->in_timeout == PROC_TIMEOUT_EXPIRE)
  {
//     printf("Time out on poll\n");
    sys_build_poll(pid, 0);
//     print_poll_syscall(pid, &(proc->sysarg));
    free(proc->sysarg.poll.fd_list);
    proc->in_timeout = PROC_NO_TIMEOUT;
    return 1;
  }
  return match;
}


int process_handle_active(pid_t pid)
{
  int status;
  process_descriptor* proc = process_get_descriptor(pid);
  int proc_state = process_get_state(proc);
  
  if(proc_state & PROC_SELECT)
  {
    //if the select match changment we have to run the child
    if(process_select_call(pid))
    {
      if(proc->timeout != NULL)
        remove_timeout(pid);
      process_reset_state(proc);
    }
    else
      return PROCESS_ON_MEDIATION;
  }
  else if(proc_state & PROC_POLL)
  {
    if(process_poll_call(pid))
    {
      if(proc->timeout != NULL)
        remove_timeout(pid);
      process_reset_state(proc);
    }
    else
      return PROCESS_ON_MEDIATION;
  }
  else if(proc_state & PROC_CONNECT)
  {
    return PROCESS_ON_MEDIATION;
  }
  else if(proc_state & PROC_ACCEPT_IN)
  {
    pid_t conn_pid = process_accept_in_call(pid, &proc->sysarg);
    if(conn_pid)
      add_to_sched_list(conn_pid); //We have to add conn_pid to the schedule list
    else
      return PROCESS_ON_MEDIATION;
  }
  else if(proc_state & PROC_RECVFROM_OUT)
    process_recvfrom_out_call(pid);
  
  else if(proc_state & PROC_READ_OUT)
    process_read_out_call(pid);
  
  else if(proc_state == PROC_RECVFROM_IN)
    THROW_IMPOSSIBLE;

  else if(proc_state == PROC_READ_IN)
    THROW_IMPOSSIBLE;
  
  else if(proc_state == PROC_RECVMSG_IN)
    THROW_IMPOSSIBLE;
    
  else if(proc_state & PROC_RECVMSG_OUT)
    process_recvmsg_out_call(pid);
  
  ptrace_resume_process(pid);
  
  if(waitpid(pid, &status, 0) < 0)
  {
    fprintf(stderr, " [%d] waitpid %s %d\n", pid, strerror(errno), errno);
    exit(1);
  }
  return process_handle( pid, status);
}


int process_recv_in_call(int pid, int fd)
{
  process_descriptor *proc = process_get_descriptor(pid);
//   fprintf(stderr, "[%d]Try to see if socket %d recv something\n", pid, fd);
  if(proc->fd_list[fd]==NULL)
    return 0;
  
  if(!socket_network(pid, fd))
    return 1;

  int status = comm_get_socket_state(get_infos_socket(pid, fd));
//   printf("socket status %d %d\n", status, status & SOCKET_READ_OK || status & SOCKET_CLOSED);
  return (status & SOCKET_READ_OK || status & SOCKET_CLOSED || status & SOCKET_SHUT);
}

void process_recvfrom_out_call(int pid)
{
  process_descriptor *proc = process_get_descriptor(pid);
  recvfrom_arg_t arg = &(proc->sysarg.recvfrom);
  //   fprintf(stderr, "[%d]Try to see if socket %d recv something\n", pid, fd);
  if(proc->fd_list[arg->sockfd]==NULL)
    return;
  
  if(!socket_network(pid, arg->sockfd))
    return;
  
  process_reset_state(proc);
  print_recvfrom_syscall(pid, &(proc->sysarg));
  sys_build_recvfrom(pid, &(proc->sysarg));
  
}

void process_read_out_call(pid_t pid)
{
  process_descriptor *proc = process_get_descriptor(pid);
//   read_arg_t arg = &(proc->sysarg.read);
  process_reset_state(proc);
  sys_build_read(pid, &(proc->sysarg));
}

void process_recvmsg_out_call(pid_t pid)
{
  process_descriptor *proc = process_get_descriptor(pid);
  sys_build_recvmsg(pid, &(proc->sysarg));
  process_reset_state(proc);
}


//Return 0 if nobody wait or the pid of the one who wait
int process_accept_in_call(pid_t pid, syscall_arg_u* sysarg)
{
  
  accept_arg_t arg = &(sysarg->accept);
  //We try to find here if there's a connection to accept
  if(comm_has_connect_waiting(get_infos_socket(pid, arg->sockfd)))
  {
    pid_t conn_pid = comm_accept_connect(get_infos_socket(pid, arg->sockfd));
    process_descriptor* conn_proc = process_get_descriptor(conn_pid);
    
    int conn_state = process_get_state(conn_proc);
    if(conn_state & PROC_CONNECT)
    {
      add_to_sched_list(conn_pid);
      process_reset_state(conn_proc);
    }
    
    //Now we rebuild the syscall.
    process_descriptor* proc = process_get_descriptor(pid);
    int new_fd = ptrace_record_socket(pid);
    printf("New socket %d\n", new_fd);
    arg->ret = new_fd;
    ptrace_neutralize_syscall(pid);
    process_set_out_syscall(proc);
    sys_build_accept(pid, sysarg);
    
    process_accept_out_call(pid, sysarg);
    
    return conn_pid;
  }
  else
  {
    process_descriptor* proc = process_get_descriptor(pid);
//     printf("Communication wait\n");
    process_set_state(proc, PROC_ACCEPT_IN);
    return 0;
  }
}

void process_accept_out_call(pid_t pid, syscall_arg_u* sysarg)
{
  accept_arg_t arg = &(sysarg->accept);
  
  if(arg->ret >= 0)
  {
    int domain = get_domain_socket(pid, arg->sockfd);
    int protocol=get_protocol_socket(pid, arg->sockfd);
    
    struct infos_socket* is = register_socket(pid, arg->ret, domain, protocol);
    comm_join_on_accept(is, pid, arg->sockfd);
  }
  process_descriptor *proc = process_get_descriptor(pid);
  process_reset_state(proc);
}

void process_shutdown_call(pid_t pid, syscall_arg_u* sysarg)
{
  shutdown_arg_t arg = &(sysarg->shutdown);
  struct infos_socket *is = get_infos_socket(pid, arg->fd);
  if(is == NULL)
    return;
  comm_shutdown(is);
}


int process_handle_idle(pid_t pid)
{
//   printf("Handle idling process %d\n", pid);
  int status;
  if(waitpid(pid, &status, WNOHANG))
    return process_handle( pid, status);
  else
    return PROCESS_IDLE_STATE;
}

// int process_clone_call(pid_t pid, reg_s *arg)
// {
//   unsigned long tid = arg->ret;
//   unsigned long flags = arg->arg1;
//   
//   //Now create new process in model
//   process_clone(tid, pid, flags);
//   
//   //Now add it to the launching time table to be the next process to be launch
//   set_next_launchment(tid);
//   
//   int status;
//   
//   //wait for clone
//   waitpid(tid, &status, 0);
//   ptrace_resume_process(tid);
//   //place process to te first call after clone
//   waitpid(tid, &status, 0);
//   process_set_in_syscall(tid);
//   
//   return 0;
// }


int process_connect_in_call(pid_t pid, syscall_arg_u *sysarg)
{
  connect_arg_t arg = &(sysarg->connect);
  int domain = get_domain_socket(pid, arg->sockfd);
  
  if(domain == 2)//PF_INET
  {
//     process_descriptor *proc = process_get_descriptor(pid);
    struct sockaddr_in *sai = &(arg->sai);
    
    //We ask for a connection on the socket
    int acc_pid = comm_ask_connect(sai->sin_addr.s_addr, ntohs(sai->sin_port), pid, arg->sockfd);
    
    //if the processus waiting for connection, we add it to schedule list
    if(acc_pid)
    {
      process_descriptor *acc_proc = process_get_descriptor(acc_pid);
      int status = process_get_state(acc_proc);
      if(status == PROC_ACCEPT_IN || status == PROC_SELECT || status == PROC_POLL)
        add_to_sched_list(acc_pid);
    }
    else
      THROW_IMPOSSIBLE;
    
    
   //Now we construct the syscall result
    
    arg->ret = 0;
    
    
    //Now we try to see if the socket is blocking of not
    int flags = socket_get_flags(pid, arg->sockfd);
    if(flags & O_NONBLOCK)
      arg->ret = -115;
    else
      arg->ret = 0;
      
    ptrace_neutralize_syscall(pid);
    process_set_out_syscall(process_get_descriptor(pid));
    sys_build_connect(pid, sysarg);
    //now mark the process as waiting for conn
    
    if(flags & O_NONBLOCK)
      return 0;

    process_set_state(process_get_descriptor(pid), PROC_CONNECT);
    return 1;
  }
  else
    return 0;
}

void process_connect_out_call(pid_t pid, syscall_arg_u *sysarg)
{
  process_descriptor *proc = process_get_descriptor(pid);
  process_reset_state(proc);
}

int process_bind_call(pid_t pid, syscall_arg_u *sysarg)
{
  bind_arg_t arg = &(sysarg->bind);
  set_localaddr_port_socket(pid,arg->sockfd,inet_ntoa(arg->sai.sin_addr),ntohs(arg->sai.sin_port));
  arg->ret=0;
  
  ptrace_neutralize_syscall(pid);
  sys_build_bind(pid, sysarg);
  process_set_out_syscall(process_get_descriptor(pid));
  return 0;
}

int process_socket_call(pid_t pid, syscall_arg_u *arg)
{
  socket_arg_t sock = &(arg->socket);
  if (sock->ret>0) 
    register_socket(pid,sock->ret,sock->domain,sock->protocol);
  return 0;
}

void process_setsockopt_syscall(pid_t pid, syscall_arg_u *sysarg)
{
  setsockopt_arg_t arg = &(sysarg->setsockopt);
  //TODO real gestion of setsockopt with warn
  arg->ret=0;
 
  if(arg->optname == SO_REUSEADDR)
    socket_set_option(pid, arg->sockfd, SOCK_OPT_REUSEADDR, *((int*)arg->optval));
  else
    XBT_WARN("Option non supported by Simterpose.\n");
  
  
  ptrace_neutralize_syscall(pid);
  sys_build_setsockopt(pid, sysarg);
  process_set_out_syscall(process_get_descriptor(pid));
}


void process_getsockopt_syscall(pid_t pid, syscall_arg_u *sysarg)
{
  getsockopt_arg_t arg = &(sysarg->getsockopt);
  
  arg->ret = 0;
  if(arg->optname == SO_REUSEADDR)
  {
    arg->optlen = sizeof(int);
    arg->optval = malloc(arg->optlen);
    *((int*)arg->optval)=socket_get_option(pid, arg->sockfd, SOCK_OPT_REUSEADDR);
  }
  else
  {
    XBT_WARN("Option non supported by Simterpose.\n");
    arg->optlen = 0;
    arg->optval = NULL;
  }
  
  ptrace_neutralize_syscall(pid);
  sys_build_getsockopt(pid, sysarg);
  process_set_out_syscall(process_get_descriptor(pid));
}


int process_listen_call(pid_t pid, syscall_arg_u* sysarg)
{
  //TODO make gestion of back_log
  listen_arg_t arg = &(sysarg->listen);
  struct infos_socket* is = get_infos_socket(pid, arg->sockfd);
  comm_t comm = comm_new(is);
  comm_set_listen(comm);
  arg->ret=0;
  
  ptrace_neutralize_syscall(pid);
  sys_build_listen(pid, sysarg);
  process_set_out_syscall(process_get_descriptor(pid));
  
  return 0;
}

void process_fcntl_call(pid_t pid, syscall_arg_u* sysarg)
{
  fcntl_arg_t arg = &(sysarg->fcntl);
  
  switch(arg->cmd)
  {
    case F_SETFL:
      socket_set_flags(pid, arg->fd , arg->arg);
      return;
      break;
    
    default:
      return;
      break;
  }
  
  ptrace_neutralize_syscall(pid);
  sys_build_fcntl(pid, sysarg);
  process_set_out_syscall(process_get_descriptor(pid));
}

void process_close_call(pid_t pid, int fd)
{
  process_descriptor *proc = process_get_descriptor(pid);
  fd_s *file_desc = proc->fd_list[fd];
  if(file_desc->type == FD_SOCKET)
    socket_close(pid, fd);
  else
  {
    free(file_desc);
    proc->fd_list[fd] = NULL;
  }
}



int process_handle_mediate(pid_t pid)
{
  process_descriptor *proc = process_get_descriptor(pid);
  int state = process_get_state(proc);
  
  if(state & PROC_RECVFROM_IN)
  {
    if(process_recv_in_call(pid, proc->sysarg.recvfrom.sockfd))
    {
      int res = process_recv_call(pid, &(proc->sysarg));
      if(res == PROCESS_TASK_FOUND)
      {
        print_recvfrom_syscall(pid, &(proc->sysarg));
        ptrace_neutralize_syscall(pid);
        process_set_out_syscall(proc);
        process_end_mediation(proc);
        return PROCESS_TASK_FOUND;
      }
      else if(res == RECV_CLOSE)
      {
        print_recvfrom_syscall(pid, &(proc->sysarg));
        ptrace_neutralize_syscall(pid);
        process_set_out_syscall(proc);
        return process_handle_active(pid);
      }
    }
  }
  
  else if(state & PROC_READ_IN)
  {
    if(process_recv_in_call(pid, proc->sysarg.recvfrom.sockfd))
    {
      int res = process_recv_call(pid, &(proc->sysarg));
      if(res == PROCESS_TASK_FOUND)
      {
        print_read_syscall(pid, &(proc->sysarg));
        ptrace_neutralize_syscall(pid);
        process_set_out_syscall(proc);
        process_end_mediation(proc);
        return PROCESS_TASK_FOUND;
      }
      else if(res == RECV_CLOSE)
      {
        print_read_syscall(pid, &(proc->sysarg));
        ptrace_neutralize_syscall(pid);
        process_set_out_syscall(proc);
        return process_handle_active(pid);
      }
    }
  }
  
  else if(state & PROC_RECVMSG_IN)
  {
    if(process_recv_in_call(pid, proc->sysarg.recvfrom.sockfd))
    {
      int res = process_recv_call(pid, &(proc->sysarg));
      if(res == PROCESS_TASK_FOUND)
      {
        print_recvmsg_syscall(pid, &(proc->sysarg));
        ptrace_neutralize_syscall(pid);
        process_set_out_syscall(proc);
        process_end_mediation(proc);
        return PROCESS_TASK_FOUND;
      }
      else if(res == RECV_CLOSE)
      {
        print_recvmsg_syscall(pid, &(proc->sysarg));
        ptrace_neutralize_syscall(pid);
        process_set_out_syscall(proc);
        return process_handle_active(pid);
      }
    }
  }
  
  return PROCESS_ON_MEDIATION;
}



int process_handle(pid_t pid, int stat)
{  
  int status = stat;
  reg_s arg;
  process_descriptor *proc = process_get_descriptor(pid);
  syscall_arg_u *sysarg = &(proc->sysarg);
  while(1)
  {
    if (process_in_syscall(proc)==0) {
      process_set_in_syscall(proc);

      ptrace_get_register(pid, &arg);
      
      int state = -1;
      switch(arg.reg_orig){
        case SYS_read:
        {
          get_args_read(pid, &arg, sysarg);
          if (socket_registered(pid, arg.arg1) != -1) {
            if(!process_recv_in_call(pid, arg.arg1))
            {
              int flags = socket_get_flags(pid, arg.arg1);
              if(flags & O_NONBLOCK)
              {
                sysarg->recvfrom.ret=0;
                print_read_syscall(pid, sysarg);
                ptrace_neutralize_syscall(pid);
                process_set_out_syscall(proc);
                process_recvmsg_out_call(pid);
              }
              else
              {
                process_set_state(proc, PROC_READ);
                process_on_mediation(proc);
                state = PROCESS_ON_MEDIATION;
              }
            }
            else
            {
              int res = process_recv_call(pid, sysarg);
              if(res == PROCESS_TASK_FOUND)
              {
                print_read_syscall(pid, sysarg);
                ptrace_neutralize_syscall(pid);
                process_set_out_syscall(proc);
                process_set_state(proc, PROC_READ);
                return PROCESS_TASK_FOUND;
              }
              else if( res == RECV_CLOSE)
              {
                print_read_syscall(pid, sysarg);
                ptrace_neutralize_syscall(pid);
                process_set_out_syscall(proc);
                process_read_out_call(pid);
              }
            }
          }
        }
        break;
        
        case SYS_write:
          get_args_write(pid, &arg, sysarg);
          if (socket_registered(pid, sysarg->write.fd) != -1) {
            if(process_send_call(pid, sysarg))
            {
              ptrace_neutralize_syscall(pid);
              sys_build_sendto(pid, sysarg);
              print_write_syscall(pid, sysarg);
              process_set_out_syscall(proc);
              return PROCESS_TASK_FOUND;
            }
          }
          break;
        
        case SYS_poll:
        {
          get_args_poll(pid, &arg, sysarg);
          print_poll_syscall(pid, sysarg);
          process_descriptor* proc = process_get_descriptor(pid);
          if(sysarg->poll.timeout >=0)
            add_timeout(pid, sysarg->poll.timeout + SD_get_clock());
          else
            proc->in_timeout = 1;
          ptrace_neutralize_syscall(pid);
          process_set_out_syscall(proc);
          process_set_state(proc, PROC_POLL);
          state =  PROCESS_ON_MEDIATION;
        }
        break;
        
        case SYS_exit_group:
        {
          printf("[%d] exit_group(%ld) called \n",pid, arg.arg1);
          ptrace_detach_process(pid);
          return PROCESS_DEAD;
        }
        break;
        
        case SYS_exit:
        {
          printf("[%d] exit(%ld) called \n", pid, arg.arg1);
          ptrace_detach_process(pid);
          return PROCESS_DEAD;
        }
        break;
        
        case SYS_futex:
        {
          printf("[%d] futex_in %p %d\n", pid, (void*)arg.arg4, arg.arg2 == FUTEX_WAIT);
          //TODO add real gestion of timeout
          if(arg.arg2 == FUTEX_WAIT)
          {
            ptrace_resume_process(pid);
            return PROCESS_IDLE_STATE;
          }
        }
        break;
        
        case SYS_listen:
          get_args_listen(pid, &arg, sysarg);
          process_listen_call(pid, sysarg);
          print_listen_syscall(pid, sysarg);
          break;
          
        case SYS_bind:
          get_args_bind_connect(pid, 0, &arg, sysarg);
          process_bind_call(pid, sysarg);
          print_bind_syscall(pid, sysarg);
          break;
        
        case SYS_connect:
        {
  //         fprintf(stderr, "New connection\n");
          printf("[%d] connect_in\n", pid);
          get_args_bind_connect(pid, 0, &arg, sysarg);
          print_connect_syscall(pid, sysarg);
          if(process_connect_in_call(pid, sysarg))
            state = PROCESS_ON_MEDIATION;
        }
        break;
        
        case SYS_accept:
        {
          printf("[%d] accept_in\n", pid);
          get_args_accept(pid, &arg, sysarg);
          print_accept_syscall(pid, sysarg);
          pid_t conn_pid = process_accept_in_call(pid, sysarg);
          if(!conn_pid)
            state =  PROCESS_ON_MEDIATION;
        }
        break;
        
        case SYS_getsockopt:
          get_args_getsockopt(pid, &arg, sysarg);
          process_getsockopt_syscall(pid, sysarg);
          print_getsockopt_syscall(pid, sysarg);
          break;
          
        case SYS_setsockopt:
          get_args_setsockopt(pid, &arg, sysarg);
          process_setsockopt_syscall(pid, sysarg);
          print_setsockopt_syscall(pid, sysarg);
          free(sysarg->setsockopt.optval);
          break;
        
        case SYS_fcntl:
          get_args_fcntl(pid, &arg, sysarg);
          print_fcntl_syscall(pid, sysarg);
          process_fcntl_call(pid, sysarg);
          break;
          
        case SYS_select:
        {
          get_args_select(pid,&arg, sysarg);
          print_select_syscall(pid, sysarg);
          process_descriptor* proc = process_get_descriptor(pid);
          if(sysarg->select.timeout >=0)
            add_timeout(pid, sysarg->select.timeout + SD_get_clock());
          else
            proc->in_timeout = 1;
          ptrace_neutralize_syscall(pid);
          process_set_out_syscall(proc);
          process_set_state(proc, PROC_SELECT);
          state =  PROCESS_ON_MEDIATION;
        }
        break;
        
        case SYS_recvfrom:
        {
          get_args_recvfrom(pid, &arg, sysarg);
  //         fprintf(stderr, "[%d] Seeing if %d receive something\n", pid, (int)arg.arg1);
          if(!process_recv_in_call(pid, sysarg->recvfrom.sockfd))
          {
            int flags = socket_get_flags(pid, arg.arg1);
            if(flags & O_NONBLOCK)
            {
              sysarg->recvfrom.ret=0;
              print_read_syscall(pid, sysarg);
              ptrace_neutralize_syscall(pid);
              process_set_out_syscall(proc);
              process_recvmsg_out_call(pid);
            }
            else
            {
              process_set_state(proc, PROC_RECVFROM);
              process_on_mediation(proc);
              state = PROCESS_ON_MEDIATION;
            }
          }
          else
          {
            int res = process_recv_call(pid, sysarg);
            if(res == PROCESS_TASK_FOUND)
            {
              ptrace_neutralize_syscall(pid);
              process_set_out_syscall(proc);
              process_set_state(proc, PROC_RECVFROM);
              return PROCESS_TASK_FOUND;
            }
            else if(res == RECV_CLOSE)
            {
              print_read_syscall(pid, sysarg);
              ptrace_neutralize_syscall(pid);
              process_set_out_syscall(proc);
              process_recvfrom_out_call(pid);
            }
          }
        }
        break;
        
        case SYS_sendmsg:
          get_args_sendmsg(pid, &arg, sysarg);
          if(process_send_call(pid, sysarg))
          {
            ptrace_neutralize_syscall(pid);
            sys_build_sendmsg(pid, sysarg);
            print_sendmsg_syscall(pid, sysarg);
            process_set_out_syscall(proc);
            return PROCESS_TASK_FOUND;
          }
          break;
        
        case SYS_recvmsg:
        {
          get_args_recvmsg(pid, &arg, sysarg);
          
          if(!process_recv_in_call(pid, sysarg->recvmsg.sockfd))
          {
            int flags = socket_get_flags(pid, arg.arg1);
            if(flags & O_NONBLOCK)
            {
              sysarg->recvmsg.ret=0;
              print_read_syscall(pid, sysarg);
              ptrace_neutralize_syscall(pid);
              process_set_out_syscall(proc);
              process_recvmsg_out_call(pid);
            }
            else
            {
              process_set_state(proc, PROC_RECVMSG);
              process_on_mediation(proc);
              state = PROCESS_ON_MEDIATION;
              //               THROW_IMPOSSIBLE;
            }
          }
          else
          {
            int res = process_recv_call(pid, sysarg);
            if(res == PROCESS_TASK_FOUND)
            {
              ptrace_neutralize_syscall(pid);
              process_set_out_syscall(proc);
              process_set_state(proc, PROC_RECVMSG);
              return PROCESS_TASK_FOUND;
            }
            else if( res == RECV_CLOSE)
            {
              print_read_syscall(pid, sysarg);
              ptrace_neutralize_syscall(pid);
              process_set_out_syscall(proc);
              process_recvmsg_out_call(pid);
            }
          }
        }
        break;
        
        case SYS_sendto:
          get_args_sendto(pid, &arg, sysarg);
          
          if(process_send_call(pid, sysarg))
          {
            ptrace_neutralize_syscall(pid);
            sys_build_sendto(pid, sysarg);
            print_sendto_syscall(pid, sysarg);
            process_set_out_syscall(proc);
            return PROCESS_TASK_FOUND;
          }
          print_sendto_syscall(pid, sysarg);
          break;
      }
      //No verify if we have compuation task to simulate.
      if(calculate_computation_time(pid))
      {
        //if we have computation to simulate
        schedule_computation_task(pid);
        process_on_simulation(proc, 1);
        state = PROCESS_ON_COMPUTATION;
      }
      if(state >= 0)
        return state;
    }
    else
    {
      process_set_out_syscall(proc);
      ptrace_get_register(pid, &arg);
      switch (arg.reg_orig) {
        
        case SYS_write:
          get_args_write(pid, &arg, sysarg);
          print_write_syscall(pid, sysarg);
          break;

        case SYS_read:
          get_args_read(pid, &arg, sysarg);
          print_read_syscall(pid, sysarg);
          if (socket_registered(pid, sysarg->read.fd) != -1) {
            if(process_recv_call(pid, sysarg) == PROCESS_TASK_FOUND)
              return PROCESS_TASK_FOUND;
          }
          break;

        case SYS_fork: 
          THROW_UNIMPLEMENTED;//Fork are not handle yet
          break;
          
        case SYS_poll:
          THROW_IMPOSSIBLE;
          break;
          
        case SYS_open:
        {
          printf("[%d] open(\"...\",", pid);
          switch (arg.arg2) {
            case 0: printf("O_RDONLY"); break;
            case 1: printf("O_WRONLY"); break;
            case 2: printf("O_RDWR"); break;
            default :printf("no_flags");break;
          }    
          printf(") = %ld\n", arg.ret);
          if((int)arg.ret >= 0)
          {
            fd_s *file_desc = malloc(sizeof(fd_s));
            file_desc->fd=(int)arg.ret;
            file_desc->proc=proc;
            file_desc->type = FD_CLASSIC;
            proc->fd_list[(int)arg.ret]=file_desc;
          }
        }
        break;
        
        case SYS_clone:
          THROW_UNIMPLEMENTED;
          if(arg.ret < MAX_PID)
          {
            process_clone_call(pid, &arg);
            return PROCESS_IDLE_STATE;
          }
          else
            process_set_in_syscall(proc);
          break;
          
        case SYS_close: 
          printf("[%d] close(%ld) = %ld\n",pid, arg.arg1,arg.ret);
          process_close_call(pid, (int)arg.arg1);
          break;
          
        case SYS_dup:
          printf("[%d] dup(%ld) = %ld\n",pid,arg.arg1,arg.ret);
//           THROW_UNIMPLEMENTED; //Dup are not handle yet
          break;
          
        case SYS_dup2:
          printf("[%d] dup2(%ld, %ld) = %ld\n", pid, arg.arg1, arg.arg2, arg.ret);
          THROW_UNIMPLEMENTED; //Dup are not handle yet
          break;
          
        case SYS_execve:
          printf("[%d] execve called\n", pid);
          THROW_UNIMPLEMENTED; //
          break;
              
              
        case SYS_fcntl:
          get_args_fcntl(pid, &arg, sysarg);
          print_fcntl_syscall(pid, sysarg);
          break;
          
          
        case SYS_select: 
          THROW_IMPOSSIBLE;
          break;
          
        case SYS_socket: 
          get_args_socket(pid, &arg, sysarg);
          print_socket_syscall(pid, sysarg);
          process_socket_call(pid, sysarg);
          break;
          
        case SYS_bind:
          get_args_bind_connect(pid, 0, &arg, sysarg);
          print_bind_syscall(pid, sysarg);
          break;
          
        case SYS_connect:
          get_args_bind_connect(pid, 1, &arg, sysarg);
          print_connect_syscall(pid, sysarg);
          break;
          
        case SYS_accept:
          get_args_accept(pid, &arg, sysarg);
          print_accept_syscall(pid, sysarg);
          printf("Here\n");
          break;
          
        case SYS_listen:
          THROW_IMPOSSIBLE;
          break;
              
        case SYS_sendto:
          get_args_sendto(pid, &arg, sysarg);
          print_sendto_syscall(pid, sysarg);
          break;
          
        case SYS_recvfrom:
          THROW_IMPOSSIBLE;
          break;
          
        case SYS_sendmsg:
          get_args_sendmsg(pid, &arg, sysarg);
          print_sendmsg_syscall(pid, sysarg);
          break;
          
        case SYS_recvmsg:
          get_args_recvmsg(pid, &arg, sysarg);
          print_recvmsg_syscall(pid, sysarg);
          if(process_recv_call(pid, sysarg) == PROCESS_TASK_FOUND)
            return PROCESS_TASK_FOUND;
          break;
          
        case SYS_shutdown:
          get_args_shutdown(pid, &arg, sysarg);
          print_shutdown_syscall(pid, sysarg);
          process_shutdown_call(pid, sysarg);
          break;
              
        case SYS_getsockopt:
          get_args_getsockopt(pid, &arg, sysarg);
          print_getsockopt_syscall(pid, sysarg);
          break;
          
        case SYS_setsockopt:
          get_args_setsockopt(pid, &arg, sysarg);
          print_setsockopt_syscall(pid, sysarg);
          break;
                
        
        default :
          printf("[%d] Unhandle syscall %s = %ld\n", pid, syscall_list[arg.reg_orig], arg.ret);
          break;
            
      }
    }
    ptrace_resume_process(pid);
    
    //waitpid sur le fils
    waitpid(pid, &status, 0);
  }
  
  THROW_IMPOSSIBLE; //There's no way to quit the loop
  
  return 0;
}
