/*
** Copyright (c) 2005 - present, Alexis Megas.
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
** -- System Includes --
*/

#include <arpa/inet.h>
#include <limits.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/types.h>

/*
** -- Local Includes --
*/

#define PIDFILE "/var/run/ez-ntpc.pid"

#include "ez-common.h"

void onalarm(int);

void onalarm(int notused)
{
  (void) notused;
}

int main(int argc, char *argv[])
{
  char buffer[2 * sizeof(long unsigned int) + 64];
  char *endptr;
  char rd_buffer[16];
  char remote_host[128];
  char *tmp = 0;
  int err = 0;
  int goodtime = 0;
  int i = 0;
  int n = 0;
  long port_num = -1;
  ssize_t rc = 0;
  struct stat st;
  struct timeval home_tp;
  struct timeval delta_tp;
  struct timeval server_tp;
  struct sigaction act;
  struct sockaddr_in servaddr;

  for(i = 0; i < argc; i++)
    if(argv && argv[i] && strcmp(argv[i], "--disable-all-logs") == 0)
      {
	disable_all_logs = 1;
	break;
      }

  if(disable_all_logs == 0)
    {
      openlog("ez-ntpc", LOG_PID | LOG_CONS, LOG_USER);
      setlogmask(LOG_UPTO(LOG_INFO));
    }

  if(stat(PIDFILE, &st) == 0)
    {
      if(disable_all_logs == 0)
	syslog(LOG_ERR, "%s already exists, exiting", PIDFILE);

      fprintf(stderr, "%s already exists, exiting.\n", PIDFILE);
      return EXIT_FAILURE;
    }

  memset(remote_host, 0, sizeof(remote_host));

  for(; *argv != 0; argv++)
    if(strcmp(*argv, "--host") == 0)
      {
	argv++;

	if(*argv != 0)
	  {
	    memset(remote_host, 0, sizeof(remote_host));
	    n = snprintf(remote_host, sizeof(remote_host), "%s", *argv);

	    if(!(n > 0 && n < (int) sizeof(remote_host)))
	      memset(remote_host, 0, sizeof(remote_host));
	  }
	else
	  {
	    if(disable_all_logs == 0)
	      syslog(LOG_ERR, "%s",
		     "undefined remote host IP address, exiting");

	    fprintf(stderr, "%s",
		    "Undefined remote host IP address, exiting.");
	    return EXIT_FAILURE;
	  }
      }
    else if(strcmp(*argv, "--port") == 0)
      {
	argv++;

	if(*argv != 0)
	  {
	    port_num = strtol(*argv, &endptr, 10);

	    if(errno == EINVAL || errno == ERANGE || endptr == *argv)
	      {
		if(disable_all_logs == 0)
		  syslog(LOG_ERR, "%s", "strtol() failure, exiting");

		fprintf(stderr, "%s", "strtol() failure, exiting.\n");
		return EXIT_FAILURE;
	      }
	  }
	else
	  {
	    if(disable_all_logs == 0)
	      syslog(LOG_ERR, "%s", "undefined port, exiting");

	    fprintf(stderr, "%s", "Undefined port, exiting.\n");
	    return EXIT_FAILURE;
	  }
      }

  if(port_num <= 0 || port_num > 65535 || strlen(remote_host) == 0)
    {
      if(disable_all_logs == 0)
	syslog(LOG_ERR, "%s",
	       "missing, or invalid, remote port number or "
	       "remote hostname, exiting");

      fprintf(stderr, "%s",
	      "Missing, or invalid, remote port number or "
	      "remote hostname, exiting.\n");
      return EXIT_FAILURE;
    }

  /*
  ** Become a daemon process.
  */

  turn_into_daemon();

  if(disable_all_logs == 0)
    {
      openlog("ez-ntpc", LOG_PID | LOG_CONS, LOG_USER);
      setlogmask(LOG_UPTO(LOG_INFO));
    }

  /*
  ** Some initialization required.
  */

  preconnect_init();

  /*
  ** Establish handlers for SIGALRM.
  */

  act.sa_handler = onalarm;
  sigemptyset(&act.sa_mask);
  act.sa_flags = 0;

  if(sigaction(SIGALRM, &act, 0) != 0)
    {
      err = errno;

      if(disable_all_logs == 0)
	syslog(LOG_ERR, "sigaction() failed, %s", strerror(err));

      fprintf(stderr, "sigaction() failed, %s.\n", strerror(err));
    }

  /*
  ** Establish a connection to the remote host.
  */

  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_addr.s_addr = inet_addr(remote_host);
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons((uint16_t) port_num);

  while(terminated < 1)
    {
      while((sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
	{
	  if(disable_all_logs == 0)
	    syslog(LOG_ERR, "socket() failed, %s, "
		   "trying again in 5 seconds", strerror(errno));

	  sleep(5);
	}

      alarm(8);

      if(connect(sock_fd, (const struct sockaddr *) &servaddr,
		 sizeof(servaddr)) == -1)
	{
	  alarm(0);

	  if(disable_all_logs == 0)
	    syslog(LOG_ERR, "connect() failed, %s, "
		   "trying again in 5 seconds", strerror(errno));

	  close(sock_fd);
	  sock_fd = -1;
	  sleep(5);
	  continue;
	}
      else
	alarm(0);

      if(terminated > 0)
	break;

      memset(buffer, 0, sizeof(buffer));

      for(goodtime = 0;;)
	{
	  if(strnlen(buffer, sizeof(buffer)) > 2 &&
	     strstr(buffer, "\r\n") != 0)
	    break;

	  alarm(8);
	  memset(rd_buffer, 0, sizeof(rd_buffer));

	  if((rc = recv(sock_fd, rd_buffer, sizeof(rd_buffer) - 1,
			MSG_PEEK)) > 0)
	    {
	      alarm(0);

	      if((size_t) rc > sizeof(rd_buffer) - 1)
		rc = sizeof(rd_buffer) - 1;

	      memset(rd_buffer, 0, sizeof(rd_buffer));
	      alarm(8);
	      rc = recv(sock_fd, rd_buffer, (size_t) rc, MSG_WAITALL);
	      alarm(0);
	    }
	  else
	    alarm(0);

	  if(rc > 0)
	    {
	      if(sizeof(buffer) > strnlen(buffer, sizeof(buffer)))
		strncat
		  (buffer, rd_buffer,
		   sizeof(buffer) - strnlen(buffer,
					    sizeof(buffer) - 1) - 1);
	      else
		break;
	    }
	  else
	    break;

	  if(strnlen(buffer, sizeof(buffer)) >
	     2 * sizeof(long unsigned int) + 16)
	    break;
	}

      if(strnlen(buffer, sizeof(buffer)) > 2 && strstr(buffer, "\r\n") != 0)
	goodtime = 1;

      if(goodtime == 0)
	{
	  shutdown(sock_fd, SHUT_RDWR);
	  close(sock_fd);
	  sock_fd = -1;

	  if(disable_all_logs == 0)
	    syslog(LOG_ERR, "incorrect time (%s)", buffer);

	  sleep(5);
	  continue;
	}

      tmp = strtok(buffer, ",");

      if(tmp != 0)
	{
	  server_tp.tv_sec = strtol(tmp, &endptr, 10);

	  if(errno == EINVAL || errno == ERANGE)
	    {
	      shutdown(sock_fd, SHUT_RDWR);
	      close(sock_fd);
	      sock_fd = -1;
	      sleep(5);
	      continue;
	    }
	  else if(endptr == tmp)
	    {
	      shutdown(sock_fd, SHUT_RDWR);
	      close(sock_fd);
	      sock_fd = -1;
	      sleep(5);
	      continue;
	    }
	}
      else
	{
	  shutdown(sock_fd, SHUT_RDWR);
	  close(sock_fd);
	  sock_fd = -1;
	  sleep(5);
	  continue;
	}

      tmp = strtok(0, "\r\n");

      if(tmp != 0)
	{
#if defined(__APPLE__)
	  server_tp.tv_usec = atoi(tmp);
#else
	  server_tp.tv_usec = strtol(tmp, &endptr, 10);
#endif

	  if(errno == EINVAL || errno == ERANGE)
	    {
	      shutdown(sock_fd, SHUT_RDWR);
	      close(sock_fd);
	      sock_fd = -1;
	      sleep(5);
	      continue;
	    }
	  else if(endptr == tmp)
	    {
	      shutdown(sock_fd, SHUT_RDWR);
	      close(sock_fd);
	      sock_fd = -1;
	      sleep(5);
	      continue;
	    }
	}
      else
	{
	  shutdown(sock_fd, SHUT_RDWR);
	  close(sock_fd);
	  sock_fd = -1;
	  sleep(5);
	  continue;
	}

      if(gettimeofday(&home_tp, 0) == 0)
	{
	  if(labs(home_tp.tv_sec - server_tp.tv_sec) >= 1)
	    {
	      if(labs(home_tp.tv_sec - server_tp.tv_sec) <= 15)
		{
		  if(settimeofday(&server_tp, 0) != 0)
		    {
		      if(disable_all_logs == 0)
			syslog(LOG_ERR, "settimeofday() failed, %s",
			       strerror(errno));
		    }
		  else if(disable_all_logs == 0)
		    syslog(LOG_INFO, "%s",
			   "adjusted system time (settimeofday())");
		}
	      else if(disable_all_logs == 0)
		syslog(LOG_INFO, "%s", "time beyond acceptable limits");
	    }
	  else if(labs(home_tp.tv_usec - server_tp.tv_usec) >= 5)
	    {
	      delta_tp.tv_sec = server_tp.tv_sec - home_tp.tv_sec;
	      delta_tp.tv_usec = server_tp.tv_usec - home_tp.tv_usec;

	      if(adjtime(&delta_tp, 0) != 0)
		{
		  if(disable_all_logs == 0)
		    syslog(LOG_ERR, "adjtime() failed, %s",
			   strerror(errno));
		}
	      else if(disable_all_logs == 0)
		syslog(LOG_INFO, "%s", "adjusted system time (adjtime())");
	    }
	  else if(disable_all_logs == 0)
	    syslog(LOG_INFO, "%s", "time beyond acceptable limits");
	}
      else if(disable_all_logs == 0)
	syslog(LOG_ERR, "gettimeofday() failed, %s", strerror(errno));

      shutdown(sock_fd, SHUT_RDWR);
      close(sock_fd);
      sock_fd = -1;
      sleep(5);
    }

  return EXIT_SUCCESS;
}
