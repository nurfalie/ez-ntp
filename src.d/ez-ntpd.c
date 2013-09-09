/*
** Copyright (c) 2005, 2006, 2013 Alexis Megas.
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

#include <netdb.h>
#include <pthread.h>

/*
** -- Local Includes --
*/

#define PIDFILE "/var/run/ez-ntpd.pid"

#include <ez-common.h>

static void *thread_fun(void *);

int main(int argc, char *argv[])
{
  int i = 0;
  int rc = 0;
  int conn_fd = -1;
  int tmpint = 0;
  pthread_t thread = 0;
  socklen_t len = 0;
  struct stat st;
  struct servent *serv = 0;
  struct sockaddr client;
  struct sockaddr_in servaddr;

  for(i = 0; i < argc; i++)
    if(strcmp(argv[i], "--disable_all_logs") == 0)
      disable_all_logs = 1;

  if(disable_all_logs == 0)
    {
      openlog("ez-ntpd", LOG_PID, LOG_USER);
      setlogmask(LOG_UPTO(LOG_INFO));
    }

  if(stat(PIDFILE, &st) == 0)
    {
      if(disable_all_logs == 0)
	syslog(LOG_ERR, "%s already exists, exiting", PIDFILE);

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

  /*
  ** Attach to a well-known port and await incoming requests.
  */

  if((serv = getservbyname("ez-ntp", "tcp")) == 0)
    {
      if(disable_all_logs == 0)
	syslog(LOG_ERR, "ez-ntp not found in /etc/services, exiting");

      return EXIT_FAILURE;
    }

  if((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
      if(disable_all_logs == 0)
	syslog(LOG_ERR, "socket() failed, %s. exiting", strerror(errno));

      return EXIT_FAILURE;
    }

  tmpint = 1;

  if(setsockopt(sock_fd, SOL_SOCKET, SO_REUSEADDR, &tmpint, sizeof(int)) != 0)
    {
      if(disable_all_logs == 0)
	syslog(LOG_ERR, "setsockopt() failed, %s. exiting", strerror(errno));

      return EXIT_FAILURE;
    }

  /*
  ** Issue a bind() call.
  */

  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = serv->s_port;

  if(bind(sock_fd, (const struct sockaddr *) &servaddr, sizeof(servaddr)) != 0)
    {
      if(disable_all_logs == 0)
	syslog(LOG_ERR, "bind() failed, %s. exiting", strerror(errno));

      return EXIT_FAILURE;
    }

  /*
  ** Start accepting connections.
  */

  if(listen(sock_fd, SOMAXCONN) != 0)
    {
      if(disable_all_logs == 0)
	syslog(LOG_ERR, "listen() failed, %s. exiting", strerror(errno));

      return EXIT_FAILURE;
    }

  for(;;)
    {
      len = sizeof(client);

      if((conn_fd = accept(sock_fd, &client, &len)) >= 0)
	{
	  if((rc = pthread_create(&thread, (const pthread_attr_t *) 0,
				  thread_fun, (void *) &conn_fd)) != 0)
	    {
	      if(disable_all_logs == 0)
		syslog
		  (LOG_ERR, "pthread_create() failed, error code = %d", rc);

	      (void) close(conn_fd);
	    }
	}
      else
	{
	  if(disable_all_logs == 0)
	    syslog(LOG_ERR, "accept() failed, %s", strerror(errno));

	  (void) sleep(1);
	}
    }

  return EXIT_SUCCESS;
}

static void *thread_fun(void *arg)
{
  int fd = *((int *) arg);
  char wr_buffer[128];
  ssize_t rc = 0;
  struct timeval tp;

  (void) pthread_detach(pthread_self());
  (void) memset(wr_buffer, 0, sizeof(wr_buffer));

  /*
  ** Fetch the time.
  */

  if(gettimeofday(&tp, (struct timezone *) 0) == 0)
    {
      (void) memset(wr_buffer, 0, sizeof(wr_buffer));
      (void) snprintf(wr_buffer, sizeof(wr_buffer),
		      "%lud,%lud\r\n", tp.tv_sec, tp.tv_usec);
      rc = send(fd, wr_buffer, strlen(wr_buffer), MSG_DONTWAIT);

      if(rc > 0)
	{
	  if(rc != (ssize_t) strlen(wr_buffer)) /*
						** wr_buffer will never, ever
						** contain more than SSIZE_MAX
						** bytes.
						*/
	    if(disable_all_logs == 0)
	      syslog(LOG_ERR, "not all data sent on send()");
	}
      else if(rc == -1)
	if(disable_all_logs == 0)
	  syslog(LOG_ERR, "send() failed, %s", strerror(errno));
    }
  else if(disable_all_logs == 0)
    syslog(LOG_ERR, "gettimeofday() failed, %s", strerror(errno));

  if(close(fd) != 0)
    if(disable_all_logs == 0)
      syslog(LOG_ERR, "close() failed, %s", strerror(errno));

  return (void *) 0;
}
