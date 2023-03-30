#include <sys/types.h>
#include <signal.h>
#include <unistd.h>

#include <time.h>
/*
#include "fork.h"
#include "fd.h"
#include "fmt.h"
#include "sig.h"
#include "subfd.h"
#include "readwrite.h"
*/

#include <skalibs/buffer.h>
#include <skalibs/strerr.h>
#include <skalibs/sig.h>
#include <skalibs/djbunix.h>
#include <skalibs/types.h>

#include "datetime.h"
#include "strerr.h"
#include "wait.h"
#include "substdio.h"

#define FATAL "predate: fatal: "

static char *montab[12] = {
"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"
};

char num[ULONG_FMT];
char outbuf[1024];

int main(int argc, char **argv)
{
  time_t now;
  struct tm *tm;
  struct datetime dt;
  datetime_sec utc;
  datetime_sec local;
  int minutes;
  int pi[2];
  buffer ss;
  int wstat;
  int pid;

  PROG = "predate";

  sig_ignore(SIGPIPE);

  if (!argv[1])
    strerr_dieusage(100, "predate child");

  if (pipe(pi) == -1)
    strerr_diefu1sys(111, "create pipe");

  switch(pid = fork()) {
    case -1:
      strerr_diefu1sys(111, "fork");
    case 0:
      close(pi[1]);
      if (fd_move(0,pi[0]) == -1)
        strerr_diefu1sys(111, "set up fds");
      sig_restore(SIGPIPE);
      execvp(argv[1],argv + 1);
      strerr_diefu2sys(111, "run ", argv[1]);
  }
  close(pi[0]);
  buffer_init(&ss, buffer_write, pi[1], outbuf, sizeof(outbuf));

  now = time(NULL);

  tm = gmtime(&now);
  dt.year = tm->tm_year;
  dt.mon = tm->tm_mon;
  dt.mday = tm->tm_mday;
  dt.hour = tm->tm_hour;
  dt.min = tm->tm_min;
  dt.sec = tm->tm_sec;
  utc = datetime_untai(&dt); /* utc == now, if gmtime ignores leap seconds */

  tm = localtime(&now);
  dt.year = tm->tm_year;
  dt.mon = tm->tm_mon;
  dt.mday = tm->tm_mday;
  dt.hour = tm->tm_hour;
  dt.min = tm->tm_min;
  dt.sec = tm->tm_sec;
  local = datetime_untai(&dt);

  buffer_puts(&ss,"Date: ");
  buffer_put(&ss,num,uint_fmt(num,dt.mday));
  buffer_puts(&ss," ");
  buffer_puts(&ss,montab[dt.mon]);
  buffer_puts(&ss," ");
  buffer_put(&ss,num,uint_fmt(num,dt.year + 1900));
  buffer_puts(&ss," ");
  buffer_put(&ss,num,uint0_fmt(num,dt.hour,2));
  buffer_puts(&ss,":");
  buffer_put(&ss,num,uint0_fmt(num,dt.min,2));
  buffer_puts(&ss,":");
  buffer_put(&ss,num,uint0_fmt(num,dt.sec,2));

  if (local < utc) {
    minutes = (utc - local + 30) / 60;
    buffer_puts(&ss," -");
    buffer_put(&ss,num,uint0_fmt(num,minutes / 60,2));
    buffer_put(&ss,num,uint0_fmt(num,minutes % 60,2));
  }
  else {
    minutes = (local - utc + 30) / 60;
    buffer_puts(&ss," +");
    buffer_put(&ss,num,uint0_fmt(num,minutes / 60,2));
    buffer_put(&ss,num,uint0_fmt(num,minutes % 60,2));
  }

  buffer_puts(&ss,"\n");
  buffer_copy(&ss, buffer_0);
  buffer_flush(&ss);
  close(pi[1]);

  if (waitpid_nointr(pid, &wstat, 0) == -1)
    strerr_dief1sys(111, "wait failed");
  if (wait_crashed(wstat))
    strerr_dief1x(111, "child crashed");
  return wait_exitcode(wstat);
}
