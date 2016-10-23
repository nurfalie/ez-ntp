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
#include <netdb.h>
#include <pthread.h>

/*
** -- Local Includes --
*/

#define PIDFILE "/var/run/ez-ntpd.pid"

#include "ez-common.h"

static void *thread_fun(void *);

int main(int argc, char *argv[])
{
  char *endptr;
  char remote_host[128];
  int *conn_fd = 0;
  int err = 0;
  int i = 0;
  int n = 0;
  int rc = 0;
  int tmpint = 0;
  long port_num = -1;
  pthread_t thread = 0;
  socklen_t length = 0;
  struct sockaddr client;
  struct sockaddr_in servaddr;
  struct stat st;

  for(i = 0; i < argc; i++)
    if(argv && argv[i] && strcmp(argv[i], "--disable-all-logs") == 0)
      disable_all_logs = 1;
    else if(argv && argv[i] && strcmp(argv[i], "--shutdown-before-close") == 0)
      shutdown_before_close = 1;

  if(disable_all_logs == 0)
    {
      openlog("ez-ntpd", LOG_PID, LOG_USER);
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
    else if(strcmp(*argv, "--so-linger") == 0)
      {
	argv++;

	if(*argv != 0)
	  {
	    so_linger = (int) strtol(*argv, &endptr, 10);

	    if(errno == EINVAL || errno == ERANGE || endptr == *argv)
	      so_linger = -1;
	  }
      }

  if(port_num <= 0 || port_num > 65535)
    {
      if(disable_all_logs == 0)
	syslog(LOG_ERR, "%s",
	       "missing, or invalid, remote port number, exiting");

      fprintf(stderr, "%s",
	      "Missing, or invalid, remote port number, exiting.\n");
      return EXIT_FAILURE;
    }

  /*
  ** Become a daemon process.
  */

  turn_into_daemon();

  if(disable_all_logs == 0)
    {
      openlog("ez-ntpd", LOG_PID, LOG_USER);
      setlogmask(LOG_UPTO(LOG_INFO));
    }

  /*
  ** Some initialization required.
  */

  preconnect_init();

  if((sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1)
    {
      err = errno;

      if(disable_all_logs == 0)
	syslog(LOG_ERR, "socket() failed, %s, exiting", strerror(err));

      fprintf(stderr, "socket() failed, %s, exiting.\n", strerror(err));
      return EXIT_FAILURE;
    }

  tmpint = 1;

  if(setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &tmpint, sizeof(int)) != 0)
    {
      err = errno;

      if(disable_all_logs == 0)
	syslog(LOG_ERR, "setsockopt() failed, %s, exiting", strerror(err));

      fprintf(stderr, "setsockopt() failed, %s, exiting.\n", strerror(err));
      return EXIT_FAILURE;
    }

  /*
  ** Issue a bind() call.
  */

  memset(&servaddr, 0, sizeof(servaddr));

  if(strlen(remote_host) > 0)
    servaddr.sin_addr.s_addr = inet_addr(remote_host);
  else
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

  servaddr.sin_family = AF_INET;
  servaddr.sin_port = htons((uint16_t) port_num);

  if(bind(sock_fd, (const struct sockaddr *) &servaddr, sizeof(servaddr)) != 0)
    {
      err = errno;

      if(disable_all_logs == 0)
	syslog(LOG_ERR, "bind() failed, %s, exiting", strerror(err));

      fprintf(stderr, "bind() failed, %s, exiting.\n", strerror(err));
      return EXIT_FAILURE;
    }

  /*
  ** Start accepting connections.
  */

  if(listen(sock_fd, SOMAXCONN) != 0)
    {
      err = errno;

      if(disable_all_logs == 0)
	syslog(LOG_ERR, "listen() failed, %s, exiting", strerror(err));

      fprintf(stderr, "listen() failed, %s, exiting.\n", strerror(err));
      return EXIT_FAILURE;
    }

  for(;;)
    {
      conn_fd = malloc(sizeof(int));

      if(!conn_fd)
	{
	  if(disable_all_logs == 0)
	    syslog(LOG_ERR, "malloc() failed");

	  sleep(1);
	  continue;
	}

      length = sizeof(client);

      if((*conn_fd = accept(sock_fd, &client, &length)) >= 0)
	{
	  shutdown(*conn_fd, SHUT_RD);

	  if((rc = pthread_create(&thread, 0, thread_fun, conn_fd)) != 0)
	    {
	      if(disable_all_logs == 0)
		syslog
		  (LOG_ERR, "pthread_create() failed, error code = %d", rc);

	      ez_close(*conn_fd);
	      free(conn_fd);
	    }
	}
      else
	{
	  if(disable_all_logs == 0)
	    syslog(LOG_ERR, "accept() failed, %s", strerror(errno));

	  free(conn_fd);
	  sleep(1);
	}
    }

  return EXIT_SUCCESS;
}

static void *thread_fun(void *arg)
{
  char *ptr = 0;
  char wr_buffer[2 * sizeof(long unsigned int) + 64];
  int fd = -1;
  int n = 0;
  ssize_t remaining = 0;
  ssize_t rc = 0;
  struct timeval tp;

  if(arg)
    fd = *((int *) arg);

  free(arg);
  pthread_detach(pthread_self());

  if(fd < 0)
    return 0;

  /*
  ** Fetch the time.
  */

  if(gettimeofday(&tp, (struct timezone *) 0) == 0)
    {
      memset(wr_buffer, 0, sizeof(wr_buffer));
      n = snprintf(wr_buffer, sizeof(wr_buffer),
		   "%ld,%ld\r\n", (long) tp.tv_sec, (long) tp.tv_usec);

      if(!(n > 0 && n < (int) sizeof(wr_buffer)))
	goto done_label;

      ptr = wr_buffer;
      remaining = (ssize_t) strlen(wr_buffer);

      while(remaining > 0)
	{
	  rc = send(fd, ptr, (size_t) remaining, MSG_DONTWAIT);

	  if(rc <= 0)
	    {
	      if(rc == -1)
		if(disable_all_logs == 0)
		  syslog(LOG_ERR, "send() failed, %s", strerror(errno));

	      break;
	    }

	  remaining -= rc;
	  ptr += rc;
	}
    }
  else if(disable_all_logs == 0)
    syslog(LOG_ERR, "gettimeofday() failed, %s", strerror(errno));

 done_label:

  shutdown(fd, SHUT_WR);

  if(so_linger >= 0)
    {
      struct linger sol;

      sol.l_onoff = 1;
      sol.l_linger = so_linger;
      setsockopt(fd, SOL_SOCKET, SO_LINGER, &sol, sizeof(sol));
    }

  if(close(fd) != 0)
    if(disable_all_logs == 0)
      syslog(LOG_ERR, "close() failed, %s", strerror(errno));

  return 0;
}
