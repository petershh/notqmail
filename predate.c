#include <stdint.h>

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
#include <skalibs/tai.h>
#include <skalibs/djbtime.h>
#include <skalibs/lolstdio.h>

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
  tai now;
  struct tm tm;
  uint64_t utc;
  uint64_t local;
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

  tai_now(&now);

  localtm_from_tai(&tm, &now, 0);
  utc_from_localtm(&utc, &tm);

  utc_from_tai(&local, &now);
  localtm_from_tai(&tm, &now, 1);

  buffer_puts(&ss,"Date: ");
  buffer_put(&ss,num,uint_fmt(num,tm.tm_mday));
  buffer_puts(&ss," ");
  buffer_puts(&ss,montab[tm.tm_mon]);
  buffer_puts(&ss," ");
  buffer_put(&ss,num,uint_fmt(num,tm.tm_year + 1900));
  buffer_puts(&ss," ");
  buffer_put(&ss,num,uint0_fmt(num,tm.tm_hour,2));
  buffer_puts(&ss,":");
  buffer_put(&ss,num,uint0_fmt(num,tm.tm_min,2));
  buffer_puts(&ss,":");
  buffer_put(&ss,num,uint0_fmt(num,tm.tm_sec,2));

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
