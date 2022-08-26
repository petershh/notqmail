#if 0
#include "sig.h"
#include "readwrite.h"
#include "exit.h"
#include "env.h"
#include "error.h"
#include "fork.h"
#include "strerr.h"
#include "fmt.h"

#define FATAL "condredirect: fatal: "
#endif
#include <errno.h>

#include <unistd.h>
#include <signal.h>

#include <skalibs/buffer.h>
#include <skalibs/strerr2.h>
#include <skalibs/error.h>
#include <skalibs/sig.h>
#include <skalibs/djbunix.h>
#include <skalibs/env.h>
#include <skalibs/types.h>

#include "seek.h"
#include "substdio.h"
#include "qmail.h"
#include "wait.h"

struct qmail qqt;

GEN_QMAILPUT_WRITE(&qqt)
/*
char inbuf[SUBSTDIO_INSIZE];
char outbuf[1];
substdio ssin = SUBSTDIO_FDBUF(read,0,inbuf,sizeof(inbuf));
substdio ssout = SUBSTDIO_FDBUF(qmail_put_write,-1,outbuf,sizeof(outbuf));
*/
char num[ULONG_FMT];

#define USAGE "condredirect newaddress program [ arg ... ]"

int main(int argc, char **argv)
{
  char const *sender;
  char const *dtline;
  int pid;
  int wstat;
  char *qqx;
  char outbuf[1];
  buffer buffer_1q = BUFFER_INIT(qmail_put_write, -1, outbuf, sizeof(outbuf));
  PROG = "condredirect";
 
  if (argc < 3)
    strerr_dieusage(100, USAGE);
 
  pid = fork();
  if (pid == -1)
    strerr_diefu1sys(111, "fork");
  if (pid == 0) {
    execvp(argv[2], argv + 2);
    if (error_temp(errno))
      _exit(111);
    _exit(100);
  }
  if (wait_pid(pid, &wstat) == -1)
    strerr_dief1x(111, "wait failed");
  if (wait_crashed(wstat))
    strerr_dief1x(111, "child crashed");
  switch(wait_exitcode(wstat)) {
    case 0:
      break;
    case 111:
      strerr_dief1x(111, "temporary child error");
    default:
      _exit(0);
  }

  if (seek_begin(0) == -1)
    strerr_diefu1sys(111, "rewind");
  sig_catch(SIGPIPE, SIG_IGN);
 
  sender = env_get("SENDER");
  if (!sender) 
    strerr_dief1x(100, "SENDER not set");
  dtline = env_get("DTLINE");
  if (!dtline)
    strerr_dief1x(100, "DTLINE not set");
 
  if (qmail_open(&qqt) == -1)
    strerr_diefu1sys(111, "fork");
  qmail_puts(&qqt, dtline);
  if (buffer_copy(&buffer_1q, buffer_0) != 0) /* ??? */
    strerr_diefu1sys(111, "read message");
  buffer_flush(&buffer_1q);
 
  num[ulong_fmt(num, qmail_qp(&qqt))] = 0;

  qmail_from(&qqt, sender);
  qmail_to(&qqt, argv[1]);
  qqx = qmail_close(&qqt);
  if (*qqx)
    strerr_dief1x(*qqx == 'D' ? 100 : 111, qqx + 1);
  strerr_die2x(99, "qp ", num);
}
