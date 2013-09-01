/*
** Copyright (c) 2005, 2006 Alexis Megas.
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

#include <limits.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>

/*
** -- Local Includes --
*/

#define PIDFILE "/var/run/ez-ntpc.pid"

#include <ez-common.h>

void onalarm(int);

void onalarm(int notused)
{
  (void) notused;
  return;
}

int main(int argc, char *argv[])
{
  int i = 0;
  int port_num = -1;
  char *tmp = 0;
  char buffer[128];
  char *endptr;
  char rd_buffer[2];
  char remote_host[128];
  short goodtime = 0;
  short disable_log = 0;
  short disable_conn_log = 0;
  ssize_t rc = 0;
  struct stat st;
  struct timeval home_tp;
  struct timeval delta_tp;
  struct timeval server_tp;
  struct sigaction act;
  struct sockaddr_in servaddr;

  for(i = 0; i < argc; i++)
    if(strcmp(argv[i], "--disable_all_logs") == 0)
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

      return EXIT_FAILURE;
    }

  (void) memset(remote_host, '\0', sizeof(remote_host));

  for(; *argv != NULL; argv++)
    if(strcmp(*argv, "-p") == 0)
      {
	argv++;

	if(*argv != NULL)
	  port_num = atoi(*argv);
	else
	  {
	    if(disable_all_logs == 0)
	      syslog(LOG_ERR, "NULL port, exiting");

	    return EXIT_FAILURE;
	  }
      }
    else if(strcmp(*argv, "-h") == 0)
      {
	argv++;

	if(*argv != NULL)
	  {
	    (void) memset(remote_host, '\0', sizeof(remote_host));
	    (void) snprintf(remote_host, sizeof(remote_host), "%s", *argv);
	  }
	else
	  {
	    if(disable_all_logs == 0)
	      syslog(LOG_ERR, "NULL remote host IP address, exiting");

	    return EXIT_FAILURE;
	  }
      }
    else if(strcmp(*argv, "--disable_log") == 0)
      disable_log = 1;
    else if(strcmp(*argv, "--disable_conn_log") == 0)
      disable_conn_log = 1;

  if(port_num == -1 || strlen(remote_host) == 0)
    {
      if(disable_all_logs == 0)
	syslog(LOG_ERR, "missing remote port number or remote hostname, "
	       "exiting");

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
  ** Establish handlers for SIGALRM and SIGTERM.
  */

  act.sa_handler = onalarm;
  (void) sigemptyset(&act.sa_mask);
  act.sa_flags = 0;

  if(sigaction(SIGALRM, &act, (struct sigaction *) NULL) != 0)
    if(disable_all_logs == 0)
      syslog(LOG_ERR, "sigaction() failed, %s", strerror(errno));

  /*
  ** Establish a connection to the remote host.
  */

  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons((uint16_t) port_num);
  servaddr.sin_addr.s_addr = inet_addr(remote_host);

  while(terminated < 1)
    {
      while((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
	  if(disable_all_logs == 0)
	    syslog(LOG_ERR, "socket() failed, %s. "
		   "trying again in 15 seconds", strerror(errno));

	  (void) sleep(15);
	}

      if(connect(sock_fd, (const struct sockaddr *) &servaddr,
		 sizeof(servaddr)) == -1)
	{
	  if(disable_conn_log == 0 && disable_all_logs == 0)
	    syslog(LOG_ERR, "connect() failed, %s. "
		   "trying again in 15 seconds", strerror(errno));

	  (void) shutdown(sock_fd, SHUT_RDWR);
	  sock_fd = -1;
	  (void) sleep(15);
	  continue;
	}

      if(terminated > 0)
	break;

      (void) memset(buffer, '\0', sizeof(buffer));
      (void) memset(rd_buffer, '\0', sizeof(rd_buffer));
      (void) alarm(8);

      for(goodtime = 0;;)
	{
	  if((rc = recv(sock_fd, rd_buffer, 1, MSG_WAITALL)) == 0)
	    (void) sleep(1);
	  else if(rc == -1 && errno == EINTR)
	    break;
	  else if(rc == 1 && strlen(buffer) < sizeof(buffer) - 1)
	    (void) strncat(buffer, rd_buffer, 1);
	  else
	    break;

	  if(strstr(buffer, "\r\n") != NULL)
	    {
	      goodtime = 1;
	      break;
	    }
	}

      (void) alarm(0);

      if(goodtime == 0)
	{
	  (void) shutdown(sock_fd, SHUT_RDWR);
	  sock_fd = -1;
	  continue;
	}

      tmp = strtok(buffer, ",");

      if(tmp != NULL)
	{
	  server_tp.tv_sec = strtol(tmp, &endptr, 10);

	  if((errno == ERANGE && (server_tp.tv_sec == LONG_MAX ||
				  server_tp.tv_sec == LONG_MIN)) ||
	     (errno != 0 && server_tp.tv_sec == 0))
	    {
	      (void) shutdown(sock_fd, SHUT_RDWR);
	      sock_fd = -1;
	      continue;
	    }
	  else if(endptr == tmp)
	    {
	      (void) shutdown(sock_fd, SHUT_RDWR);
	      sock_fd = -1;
	      continue;
	    }
	}
      else
	{
	  (void) shutdown(sock_fd, SHUT_RDWR);
	  sock_fd = -1;
	  continue;
	}

      tmp = strtok(NULL, "\r\n");

      if(tmp != NULL)
	{
	  server_tp.tv_usec = strtol(tmp, &endptr, 10);

	  if((errno == ERANGE && (server_tp.tv_usec == LONG_MAX ||
				  server_tp.tv_usec == LONG_MIN)) ||
	     (errno != 0 && server_tp.tv_usec == 0))
	    {
	      (void) shutdown(sock_fd, SHUT_RDWR);
	      sock_fd = -1;
	      continue;
	    }
	  else if(endptr == tmp)
	    {
	      (void) shutdown(sock_fd, SHUT_RDWR);
	      sock_fd = -1;
	      continue;
	    }
	}
      else
	{
	  (void) shutdown(sock_fd, SHUT_RDWR);
	  sock_fd = -1;
	  continue;
	}

      if(gettimeofday(&home_tp, NULL) == 0)
	{
	  if(abs(server_tp.tv_sec - home_tp.tv_sec) >= 1)
	    {
	      if(settimeofday(&server_tp, NULL) != 0)
		{
		  if(disable_all_logs == 0)
		    syslog(LOG_ERR, "settimeofday() failed, %s",
			   strerror(errno));
		}
	      else if(disable_log == 0 && disable_all_logs == 0)
		syslog(LOG_INFO, "adjusted system time");
	    }
	  else if(abs(server_tp.tv_usec - home_tp.tv_usec) >= 5)
	    {
	      delta_tp.tv_sec = server_tp.tv_sec - home_tp.tv_sec;
	      delta_tp.tv_usec = server_tp.tv_usec - home_tp.tv_usec;

	      if(adjtime(&delta_tp, NULL) != 0)
		{
		  if(disable_all_logs == 0)
		    syslog(LOG_ERR, "adjtime() failed, %s",
			   strerror(errno));
		}
	      else if(disable_log == 0 && disable_all_logs == 0)
		syslog(LOG_INFO, "adjusted system time");
	    }
	}
      else if(disable_all_logs == 0)
	syslog(LOG_ERR, "gettimeofday() failed, %s", strerror(errno));

      (void) shutdown(sock_fd, SHUT_RDWR);
      sock_fd = -1;
      (void) sleep(60);
    }

  return EXIT_SUCCESS;
}
