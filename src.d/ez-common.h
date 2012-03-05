#ifndef _ez_common_h_
#define _ez_common_h_

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <strings.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <linux/socket.h>
#include <sys/resource.h>
#include <netinet/in.h>

#define VERSION 1.8.3

int sock_fd = -1;
int terminated = 0;
int disable_all_logs = 0;
void onexit(void);
void onterm(int);
void preconnect_init(void);
void turn_into_daemon(void);

void onexit(void)
{
  if(remove(PIDFILE) != 0)
    if(disable_all_logs == 0)
      syslog(LOG_ERR, "unable to remove() %s, %s", PIDFILE,
	     strerror(errno));

  if(sock_fd != -1)
    {
      if(close(sock_fd) != 0)
	{
	  if(disable_all_logs == 0)
	    syslog(LOG_ERR, "unable to close() the socket, %s",
		   strerror(errno));
	}
      else
	sock_fd = -1;
    }

  if(disable_all_logs == 0)
    closelog();
}

void onterm(int notused)
{
  (void) notused;
  terminated = 1;
  exit(EXIT_SUCCESS);
}

void preconnect_init(void)
{
  int fd = -1;
  char pidbuf[64];
  ssize_t pidbuf_len = 0;
  struct sigaction act;

  if(atexit(onexit) != 0)
    if(disable_all_logs == 0)
      syslog(LOG_ERR, "atexit() failed, %s", strerror(errno));

  /*
  ** Configure a handler for the SIGTERM signal.
  */

  act.sa_handler = onterm;
  (void) sigemptyset(&act.sa_mask);
  act.sa_flags = 0;

  if(sigaction(SIGTERM, &act, (struct sigaction *) NULL) != 0)
    if(disable_all_logs == 0)
      syslog(LOG_ERR, "sigaction() failed, %s", strerror(errno));

  if((fd = open(PIDFILE, O_EXCL | O_CREAT | O_WRONLY, S_IRUSR)) == -1)
    {
      if(disable_all_logs == 0)
	syslog(LOG_ERR, "open() failed for %s, %s. exiting",
	       PIDFILE, strerror(errno));

      exit(EXIT_FAILURE);
    }
  else
    {
      (void) memset(pidbuf, '\0', sizeof(pidbuf));
      (void) snprintf(pidbuf, sizeof(pidbuf), "%d", getpid());
      pidbuf_len = strlen(pidbuf);

      if(pidbuf_len != write(fd, pidbuf, strlen(pidbuf)))
	{
	  if(disable_all_logs == 0)
	    syslog(LOG_ERR, "write() failed for %s, %s. exiting",
		   PIDFILE, strerror(errno));

	  (void) close(fd);
	  exit(EXIT_FAILURE);
	}
    }

  if(close(fd) != 0)
    if(disable_all_logs == 0)
      syslog(LOG_ERR, "close() failed for %d (%s), %s", fd, PIDFILE,
	     strerror(errno));
}

void turn_into_daemon(void)
{
  int fd0 = 0;
  int fd1 = 0;
  int fd2 = 0;
  pid_t pid = 0;
  unsigned int i = 0;
  struct rlimit rl;

  /*
  ** Turn into a daemon.
  */

  if(getrlimit(RLIMIT_NOFILE, &rl) != 0)
    {
      if(disable_all_logs == 0)
	syslog(LOG_ERR, "getrlimit() failed, %s. exiting", strerror(errno));

      exit(EXIT_FAILURE);
    }

  (void) umask(0);

  if((pid = fork()) < 0)
    {
      if(disable_all_logs == 0)
	syslog(LOG_ERR, "fork() failed, exiting");

      exit(EXIT_FAILURE);
    }
  else if(pid != 0)
    exit(EXIT_SUCCESS);

  (void) setsid();

  if(chdir("/") != 0)
    {
      if(disable_all_logs == 0)
	syslog(LOG_ERR, "chdir() failed, %s. exiting", strerror(errno));

      exit(EXIT_FAILURE);
    }

  if(rl.rlim_max == RLIM_INFINITY)
    rl.rlim_max = 2048;

  for(i = 0; i < rl.rlim_max; i++)
    (void) close(i);

  fd0 = open("/dev/null", O_RDWR);
  fd1 = dup(0);
  fd2 = dup(1);

  if(fd0 != 0 || fd1 != 1 || fd2 != 2)
    {
      if(disable_all_logs == 0)
	syslog(LOG_ERR, "incorrect file descriptors: %d, %d, %d. exiting",
	       fd0, fd1, fd2);

      exit(EXIT_FAILURE);
    }

  if(disable_all_logs == 0)
    closelog();
}

#endif
