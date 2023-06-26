#include <unistd.h>

#include <skalibs/strerr.h>
#include <skalibs/buffer.h>
#include <skalibs/djbunix.h>

/*
#include "strerr.h"
#include "substdio.h"
#include "open.h"
#include "readwrite.h"
*/

#include "lock.h"
#include "auto_qmail.h"

#define FATAL "qmail-tcpok: fatal: "

char buf[1024]; /* XXX: must match size in tcpto_clean.c, tcpto.c */
buffer b;

int main(void)
{
  int fd;
  int i;

  if (chdir(auto_qmail) == -1)
    strerr_diefu2sys(111, "chdir to ",auto_qmail);
  if (chdir("queue/lock") == -1)
    strerr_diefu3sys(111, "chdir to ",auto_qmail,"/queue/lock");

  fd = open_write("tcpto");
  if (fd == -1)
    strerr_diefu3sys(111, "write ",auto_qmail,"/queue/lock/tcpto");
  if (lock_ex(fd) == -1)
    strerr_diefu3sys(111, "lock ",auto_qmail,"/queue/lock/tcpto");

  buffer_init(&b,buffer_write,fd,buf,sizeof(buf));
  for (i = 0;i < sizeof(buf);++i) buffer_put(&b,"",1);
  if (buffer_flush(&b) == -1)
    strerr_diefu3sys(111, "clear ",auto_qmail,"/queue/lock/tcpto");
  return 0;
}
