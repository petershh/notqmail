#include <stdio.h>

#include <sys/stat.h>
#include <unistd.h>

#include <skalibs/stralloc.h>
#include <skalibs/buffer.h>
#include <skalibs/bytestr.h>
#include <skalibs/djbunix.h>
#include <skalibs/cdbmake.h>

#include "getln.h"
#include "auto_qmail.h"

void die_temp() { _exit(111); }

void die_chdir()
{
  buffer_putsflush(buffer_2,"qmail-newu: fatal: unable to chdir\n");
  die_temp();
}
void die_nomem()
{
  buffer_putsflush(buffer_2,"qmail-newu: fatal: out of memory\n");
  die_temp();
}
void die_opena()
{
  buffer_putsflush(buffer_2,"qmail-newu: fatal: unable to open users/assign\n");
  die_temp();
}
void die_reada()
{
  buffer_putsflush(buffer_2,"qmail-newu: fatal: unable to read users/assign\n");
  die_temp();
}
void die_format()
{
  buffer_putsflush(buffer_2,"qmail-newu: fatal: bad format in users/assign\n");
  die_temp();
}
void die_opent()
{
  buffer_putsflush(buffer_2,"qmail-newu: fatal: unable to open users/cdb.tmp\n");
  die_temp();
}
void die_writet()
{
  buffer_putsflush(buffer_2,"qmail-newu: fatal: unable to write users/cdb.tmp\n");
  die_temp();
}
void die_rename()
{
  buffer_putsflush(buffer_2,"qmail-newu: fatal: unable to move users/cdb.tmp to users/cdb\n");
  die_temp();
}

cdbmaker maker = CDBMAKER_ZERO;
stralloc key = STRALLOC_ZERO;
stralloc data = STRALLOC_ZERO;

char inbuf[1024];
buffer bin;

int fd;
int fdtemp;

stralloc line = STRALLOC_ZERO;
int match;

stralloc wildchars = STRALLOC_ZERO;

int main(void)
{
  int i;
  int numcolons;

  umask(033);
  if (chdir(auto_qmail) == -1) die_chdir();

  fd = open_read("users/assign");
  if (fd == -1) die_opena();

  buffer_init(&bin,buffer_read,fd,inbuf,sizeof(inbuf));

  fdtemp = open_trunc("users/cdb.tmp");
  if (fdtemp == -1) die_opent();

  if (cdbmake_start(&maker,fdtemp) == 0) die_writet();

  if (!stralloc_copys(&wildchars,"")) die_nomem();

  for (;;) {
    if (getln(&bin,&line,&match,'\n') != 0) die_reada();
    if (line.len && (line.s[0] == '.')) break;
    if (!match) die_format();

    if (byte_chr(line.s,line.len,'\0') < line.len) die_format();
    i = byte_chr(line.s,line.len,':');
    if (i == line.len) die_format();
    if (i == 0) die_format();
    if (!stralloc_copys(&key,"!")) die_nomem();
    if (line.s[0] == '+') {
      if (!stralloc_catb(&key,line.s + 1,i - 1)) die_nomem();
      case_lowerb(key.s,key.len);
      if (i >= 2)
	if (byte_chr(wildchars.s,wildchars.len,line.s[i - 1]) == wildchars.len)
	  if (!stralloc_append(&wildchars,line.s[i - 1])) die_nomem();
    }
    else {
      if (!stralloc_catb(&key,line.s + 1,i - 1)) die_nomem();
      if (!stralloc_0(&key)) die_nomem();
      case_lowerb(key.s,key.len);
    }

    if (!stralloc_copyb(&data,line.s + i + 1,line.len - i - 1)) die_nomem();

    numcolons = 0;
    for (i = 0;i < data.len;++i)
      if (data.s[i] == ':') {
	data.s[i] = 0;
	if (++numcolons == 6)
	  break;
      }
    if (numcolons < 6) die_format();
    data.len = i;

    if (cdbmake_add(&maker,key.s,key.len,data.s,data.len) == 0) die_writet();
  }

  if (cdbmake_add(&maker,"",0,wildchars.s,wildchars.len) == 0) die_writet();

  if (cdbmake_finish(&maker) == -1) die_writet();
  if (fsync(fdtemp) == -1) die_writet();
  if (close(fdtemp) == -1) die_writet(); /* NFS stupidity */
  if (rename("users/cdb.tmp","users/cdb") == -1) die_rename();

  return 0;
}
