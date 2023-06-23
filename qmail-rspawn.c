#include <errno.h>

#include <unistd.h>

#include <skalibs/djbunix.h>
#include <skalibs/buffer.h>
#include <skalibs/env.h>
#include <skalibs/error.h>

/*
#include "fd.h"
#include "substdio.h"
#include "exit.h"
#include "fork.h"
#include "error.h"
#include "env.h"
*/

#include "spawn.h"
#include "wait.h"
#include "tcpto.h"
#include "uidgid.h"
#include "auto_uids.h"
#include "auto_users.h"

void initialize(int argc, char **argv)
{
 auto_uidq = inituid(auto_userq);
 tcpto_clean();
}

int truncreport = 0;

void report(buffer *b, int wstat, char *s, int len)
{
  int j;
  int k;
  int result;
  int orr;

  if (wait_crashed(wstat))
  { buffer_puts(b,"Zqmail-remote crashed.\n"); return; }
  switch(wait_exitcode(wstat))
  {
    case 0: break;
    case 111: buffer_puts(b,"ZUnable to run qmail-remote.\n"); return;
    default: buffer_puts(b,"DUnable to run qmail-remote.\n"); return;
  }
  if (!len)
  { buffer_puts(b,"Zqmail-remote produced no output.\n"); return; }

  result = -1;
  j = 0;
  for (k = 0;k < len;++k)
    if (!s[k])
    {
      if (s[j] == 'K') { result = 1; break; }
      if (s[j] == 'Z') { result = 0; break; }
      if (s[j] == 'D') break;
      j = k + 1;
    }

  orr = result;
  switch(s[0])
  {
    case 's': orr = 0; break;
    case 'h': orr = -1;
  }

  switch(orr)
  {
    case 1: buffer_put(b,"K",1); break;
    case 0: buffer_put(b,"Z",1); break;
    case -1: buffer_put(b,"D",1); break;
  }

  for (k = 1;k < len;)
    if (!s[k++])
    {
      buffer_puts(b,s + 1);
      if (result <= orr)
        if (k < len)
          switch(s[k])
          {
            case 'Z': case 'D': case 'K':
              buffer_puts(b,s + k + 1);
          }
      break;
    }
}

static char const *setup_qrargs(void)
{
 static char const *qr;
 if (qr) return qr;
 qr = env_get("QMAILREMOTE");
 if (qr) return qr;
 qr = "qmail-remote";
 return qr;
}

int spawn(int fdmess, int fdout, char *s, char *r, int at)
{
 int f;
 char const *(args[5]);

 args[0] = setup_qrargs();
 args[1] = r + at + 1;
 args[2] = s;
 args[3] = r;
 args[4] = 0;

 if (!(f = fork()))
  {
   if (fd_move(0,fdmess) == -1) _exit(111);
   if (fd_move(1,fdout) == -1) _exit(111);
   if (fd_copy(2,1) == -1) _exit(111);
   execvp(*args, (char *const *)args);
   if (error_temp(errno)) _exit(111);
   _exit(100);
  }
 return f;
}
