#include <stdio.h>

#include <sys/stat.h>
#include <unistd.h>

#include <skalibs/strerr.h>
#include <skalibs/buffer.h>
#include <skalibs/stralloc.h>
#include <skalibs/bytestr.h>
#include <skalibs/cdbmake.h>
#include <skalibs/djbunix.h>

#include "getln.h"
#include "auto_qmail.h"

void die_read()
{
  strerr_diefu1sys(111, "read control/morercpthosts");
}
void die_write()
{
  strerr_diefu1sys(111, "write to control/morercpthosts.tmp");
}

char inbuf[1024];
buffer bin;

int fd;
int fdtemp;

cdbmaker maker = CDBMAKER_ZERO;
stralloc line = STRALLOC_ZERO;
int match;

int main(void)
{
  PROG = "qmail-newmrh";
  umask(033);
  if (chdir(auto_qmail) == -1)
    strerr_diefu2sys(111, "chdir to ", auto_qmail);

  fd = open_read("control/morercpthosts");
  if (fd == -1) die_read();

  buffer_init(&bin,buffer_read,fd,inbuf,sizeof(inbuf));

  fdtemp = open_trunc("control/morercpthosts.tmp");
  if (fdtemp == -1) die_write();

  if (cdbmake_start(&maker, fdtemp == 0)) die_write(); 

  for (;;) {
    if (getln(&bin,&line,&match,'\n') != 0) die_read();
    case_lowerb(line.s,line.len);
    while (line.len) {
      if (line.s[line.len - 1] == ' ') { --line.len; continue; }
      if (line.s[line.len - 1] == '\n') { --line.len; continue; }
      if (line.s[line.len - 1] == '\t') { --line.len; continue; }
      if (line.s[0] != '#')
        if (cdbmake_add(&maker, line.s, line.len, "", 0) == 0)
          die_write();
      break;
    }
    if (!match) break;
  }

  if (cdbmake_finish(&maker) == 0) die_write();
  if (fsync(fdtemp) == -1) die_write();
  if (close(fdtemp) == -1) die_write(); /* NFS stupidity */
  if (rename("control/morercpthosts.tmp","control/morercpthosts.cdb") == -1)
    strerr_diefu1sys(111,"move control/morercpthosts.tmp to control/morercpthosts.cdb");

  return 0;
}
