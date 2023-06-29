#include <signal.h>

#include <skalibs/strerr.h>
#include <skalibs/strerr2.h>
#include <skalibs/buffer.h>
#include <skalibs/types.h>
#include <skalibs/env.h>
#include <skalibs/sig.h>

#include "qmail.h"

void die_nomem() { strerr_dief1x(111, "out of memory"); }

struct qmail qqt;

GEN_QMAILPUT_WRITE(&qqt)

char inbuf[BUFFER_INSIZE];
char outbuf[1];
buffer bin = BUFFER_INIT(buffer_read,0,inbuf,sizeof(inbuf));
buffer bout = BUFFER_INIT(qmail_put_write,-1,outbuf,sizeof(outbuf));

char num[ULONG_FMT];

int main(int argc, char **argv)
{
  char *sender;
  char *dtline;
  char *qqx;
  int i;
  PROG = "forward";
 
  sig_ignore(SIGPIPE);
 
  sender = env_get("NEWSENDER");
  if (!sender)
    strerr_dief1x(100, "NEWSENDER not set");
  dtline = env_get("DTLINE");
  if (!dtline)
    strerr_dief1x(100, "DTLINE not set");
 
  if (qmail_open(&qqt) == -1)
    strerr_diefu1sys(111, "fork");
  qmail_puts(&qqt,dtline);
  if (buffer_copy(&bout,&bin) != 0)
    strerr_diefu1sys(111, "read message");
  buffer_flush(&bout);

  num[ulong_fmt(num,qmail_qp(&qqt))] = 0;
 
  qmail_from(&qqt,sender);
  for (i = 1; i < argc; i++)
    qmail_to(&qqt,argv[i]);
  qqx = qmail_close(&qqt);
  if (*qqx) strerr_dief1x(*qqx == 'D' ? 100 : 111, qqx + 1);
  buffer_puts(buffer_2, "qp ");
  buffer_putsflush(buffer_2, num);
  return 0;
}
