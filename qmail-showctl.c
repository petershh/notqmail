#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>

#include <skalibs/buffer.h>
#include <skalibs/bytestr.h>
#include <skalibs/direntry.h>
#include <skalibs/stralloc.h>
#include <skalibs/types.h>

#include "control.h"
#include "constmap.h"
#include "uidgid.h"
#include "auto_uids.h"
#include "auto_users.h"
#include "auto_qmail.h"
#include "auto_break.h"
#include "auto_patrn.h"
#include "auto_spawn.h"
#include "auto_split.h"

stralloc me = STRALLOC_ZERO;
int meok;

stralloc line = STRALLOC_ZERO;
char num[ULONG_FMT];

uid_t auto_uida;
uid_t auto_uidd;
uid_t auto_uidl;
uid_t auto_uido;
uid_t auto_uidp;
uid_t auto_uidq;
uid_t auto_uidr;
uid_t auto_uids;

gid_t auto_gidn;
gid_t auto_gidq;

void safeput(char *buf, unsigned int len)
{
  char ch;

  while (len > 0) {
    ch = *buf;
    if ((ch < 32) || (ch > 126)) ch = '?';
    buffer_put(buffer_1,&ch,1);
    ++buf;
    --len;
  }
}

void do_int(char *fn, char *def, char *pre, char *post)
{
  int i;
  buffer_puts(buffer_1,"\n");
  buffer_puts(buffer_1,fn);
  buffer_puts(buffer_1,": ");
  switch(control_readint(&i,fn)) {
    case 0:
      buffer_puts(buffer_1,"(Default.) ");
      buffer_puts(buffer_1,pre);
      buffer_puts(buffer_1,def);
      buffer_puts(buffer_1,post);
      buffer_puts(buffer_1,".\n");
      break;
    case 1:
      if (i < 0) i = 0;
      buffer_puts(buffer_1,pre);
      buffer_put(buffer_1,num,uint_fmt(num,i));
      buffer_puts(buffer_1,post);
      buffer_puts(buffer_1,".\n");
      break;
    default:
      buffer_puts(buffer_1,"Oops! Trouble reading this file.\n");
      break;
  }
}

void do_str(char *fn, int flagme, char *def, char *pre)
{
  buffer_puts(buffer_1,"\n");
  buffer_puts(buffer_1,fn);
  buffer_puts(buffer_1,": ");
  switch(control_readline(&line,fn)) {
    case 0:
      buffer_puts(buffer_1,"(Default.) ");
      if (!stralloc_copys(&line,def)) {
	buffer_puts(buffer_1,"Oops! Out of memory.\n");
	break;
      }
      if (flagme && meok)
	if (!stralloc_copy(&line,&me)) {
	  buffer_puts(buffer_1,"Oops! Out of memory.\n");
	  break;
	}
    case 1:
      buffer_puts(buffer_1,pre);
      safeput(line.s,line.len);
      buffer_puts(buffer_1,".\n");
      break;
    default:
      buffer_puts(buffer_1,"Oops! Trouble reading this file.\n");
      break;
  }
}

int do_lst(char *fn, char *def, char *pre, char *post)
{
  int i;
  int j;

  buffer_puts(buffer_1,"\n");
  buffer_puts(buffer_1,fn);
  buffer_puts(buffer_1,": ");
  switch(control_readfile(&line,fn,0)) {
    case 0:
      buffer_puts(buffer_1,"(Default.) ");
      buffer_puts(buffer_1,def);
      buffer_puts(buffer_1,"\n");
      return 0;
    case 1:
      buffer_puts(buffer_1,"\n");
      i = 0;
      for (j = 0;j < line.len;++j)
	if (!line.s[j]) {
          buffer_puts(buffer_1,pre);
          safeput(line.s + i,j - i);
          buffer_puts(buffer_1,post);
          buffer_puts(buffer_1,"\n");
	  i = j + 1;
	}
      return 1;
    default:
      buffer_puts(buffer_1,"Oops! Trouble reading this file.\n");
      return -1;
  }
}

int main(void)
{
  DIR *dir;
  direntry *d;
  struct stat stmrh;
  struct stat stmrhcdb;

  auto_uida = inituid(auto_usera);
  auto_uidd = inituid(auto_userd);
  auto_uidl = inituid(auto_userl);
  auto_uido = inituid(auto_usero);
  auto_uidp = inituid(auto_userp);
  auto_uidq = inituid(auto_userq);
  auto_uidr = inituid(auto_userr);
  auto_uids = inituid(auto_users);

  auto_gidn = initgid(auto_groupn);
  auto_gidq = initgid(auto_groupq);

  buffer_puts(buffer_1,"qmail home directory: ");
  buffer_puts(buffer_1,auto_qmail);
  buffer_puts(buffer_1,".\n");

  buffer_puts(buffer_1,"user-ext delimiter: ");
  buffer_puts(buffer_1,auto_break);
  buffer_puts(buffer_1,".\n");

  buffer_puts(buffer_1,"paternalism (in decimal): ");
  buffer_put(buffer_1,num,ulong_fmt(num,(unsigned long) auto_patrn));
  buffer_puts(buffer_1,".\n");

  buffer_puts(buffer_1,"silent concurrency limit: ");
  buffer_put(buffer_1,num,ulong_fmt(num,(unsigned long) auto_spawn));
  buffer_puts(buffer_1,".\n");

  buffer_puts(buffer_1,"subdirectory split: ");
  buffer_put(buffer_1,num,ulong_fmt(num,(unsigned long) auto_split));
  buffer_puts(buffer_1,".\n");

  buffer_puts(buffer_1,"user ids: ");
  buffer_put(buffer_1,num,ulong_fmt(num,(unsigned long) auto_uida));
  buffer_puts(buffer_1,", ");
  buffer_put(buffer_1,num,ulong_fmt(num,(unsigned long) auto_uidd));
  buffer_puts(buffer_1,", ");
  buffer_put(buffer_1,num,ulong_fmt(num,(unsigned long) auto_uidl));
  buffer_puts(buffer_1,", ");
  buffer_put(buffer_1,num,ulong_fmt(num,(unsigned long) auto_uido));
  buffer_puts(buffer_1,", ");
  buffer_put(buffer_1,num,ulong_fmt(num,(unsigned long) auto_uidp));
  buffer_puts(buffer_1,", ");
  buffer_put(buffer_1,num,ulong_fmt(num,(unsigned long) auto_uidq));
  buffer_puts(buffer_1,", ");
  buffer_put(buffer_1,num,ulong_fmt(num,(unsigned long) auto_uidr));
  buffer_puts(buffer_1,", ");
  buffer_put(buffer_1,num,ulong_fmt(num,(unsigned long) auto_uids));
  buffer_puts(buffer_1,".\n");

  buffer_puts(buffer_1,"group ids: ");
  buffer_put(buffer_1,num,ulong_fmt(num,(unsigned long) auto_gidn));
  buffer_puts(buffer_1,", ");
  buffer_put(buffer_1,num,ulong_fmt(num,(unsigned long) auto_gidq));
  buffer_puts(buffer_1,".\n");

  if (chdir(auto_qmail) == -1) {
    buffer_puts(buffer_1,"Oops! Unable to chdir to ");
    buffer_puts(buffer_1,auto_qmail);
    buffer_puts(buffer_1,".\n");
    buffer_flush(buffer_1);
    _exit(111);
  }
  if (chdir("control") == -1) {
    buffer_puts(buffer_1,"Oops! Unable to chdir to control.\n");
    buffer_flush(buffer_1);
    _exit(111);
  }

  dir = opendir(".");
  if (!dir) {
    buffer_puts(buffer_1,"Oops! Unable to open current directory.\n");
    buffer_flush(buffer_1);
    _exit(111);
  }

  meok = control_readline(&me,"me");
  if (meok == -1) {
    buffer_puts(buffer_1,"Oops! Trouble reading control/me.");
    buffer_flush(buffer_1);
    _exit(111);
  }

  do_lst("badmailfrom","Any MAIL FROM is allowed.",""," not accepted in MAIL FROM.");
  do_str("bouncefrom",0,"MAILER-DAEMON","Bounce user name is ");
  do_str("bouncehost",1,"bouncehost","Bounce host name is ");
  do_int("concurrencylocal","10","Local concurrency is ","");
  do_int("concurrencyremote","20","Remote concurrency is ","");
  do_int("databytes","0","SMTP DATA limit is "," bytes");
  do_str("defaultdomain",1,"defaultdomain","Default domain name is ");
  do_str("defaulthost",1,"defaulthost","Default host name is ");
  do_str("doublebouncehost",1,"doublebouncehost","2B recipient host: ");
  do_str("doublebounceto",0,"postmaster","2B recipient user: ");
  do_str("envnoathost",1,"envnoathost","Presumed domain name is ");
  do_str("helohost",1,"helohost","SMTP client HELO host name is ");
  do_str("idhost",1,"idhost","Message-ID host name is ");
  do_str("localiphost",1,"localiphost","Local IP address becomes ");
  do_lst("locals","Messages for me are delivered locally.","Messages for "," are delivered locally.");
  do_str("me",0,"undefined! Uh-oh","My name is ");
  do_lst("percenthack","The percent hack is not allowed.","The percent hack is allowed for user%host@",".");
  do_str("plusdomain",1,"plusdomain","Plus domain name is ");
  do_lst("qmqpservers","No QMQP servers.","QMQP server: ",".");
  do_int("queuelifetime","604800","Message lifetime in the queue is "," seconds");

  if (do_lst("rcpthosts","SMTP clients may send messages to any recipient.","SMTP clients may send messages to recipients at ","."))
    do_lst("morercpthosts","No effect.","SMTP clients may send messages to recipients at ",".");
  else
    do_lst("morercpthosts","No rcpthosts; morercpthosts is irrelevant.","No rcpthosts; doesn't matter that morercpthosts has ",".");
  /* XXX: check morercpthosts.cdb contents */
  buffer_puts(buffer_1,"\nmorercpthosts.cdb: ");
  if (stat("morercpthosts",&stmrh) == -1)
    if (stat("morercpthosts.cdb",&stmrhcdb) == -1)
      buffer_puts(buffer_1,"(Default.) No effect.\n");
    else
      buffer_puts(buffer_1,"Oops! morercpthosts.cdb exists but morercpthosts doesn't.\n");
  else
    if (stat("morercpthosts.cdb",&stmrhcdb) == -1)
      buffer_puts(buffer_1,"Oops! morercpthosts exists but morercpthosts.cdb doesn't.\n");
    else
      if (stmrh.st_mtime > stmrhcdb.st_mtime)
        buffer_puts(buffer_1,"Oops! morercpthosts.cdb is older than morercpthosts.\n");
      else
        buffer_puts(buffer_1,"Modified recently enough; hopefully up to date.\n");

  do_str("smtpgreeting",1,"smtpgreeting","SMTP greeting: 220 ");
  do_lst("smtproutes","No artificial SMTP routes.","SMTP route: ","");
  do_int("timeoutconnect","60","SMTP client connection timeout is "," seconds");
  do_int("timeoutremote","1200","SMTP client data timeout is "," seconds");
  do_int("timeoutsmtpd","1200","SMTP server data timeout is "," seconds");
  do_lst("virtualdomains","No virtual domains.","Virtual domain: ","");

  while ((d = readdir(dir))) {
    if (str_equal(d->d_name,".")) continue;
    if (str_equal(d->d_name,"..")) continue;
    if (str_equal(d->d_name,"bouncefrom")) continue;
    if (str_equal(d->d_name,"bouncehost")) continue;
    if (str_equal(d->d_name,"badmailfrom")) continue;
    if (str_equal(d->d_name,"bouncefrom")) continue;
    if (str_equal(d->d_name,"bouncehost")) continue;
    if (str_equal(d->d_name,"concurrencylocal")) continue;
    if (str_equal(d->d_name,"concurrencyremote")) continue;
    if (str_equal(d->d_name,"databytes")) continue;
    if (str_equal(d->d_name,"defaultdomain")) continue;
    if (str_equal(d->d_name,"defaulthost")) continue;
    if (str_equal(d->d_name,"doublebouncehost")) continue;
    if (str_equal(d->d_name,"doublebounceto")) continue;
    if (str_equal(d->d_name,"envnoathost")) continue;
    if (str_equal(d->d_name,"helohost")) continue;
    if (str_equal(d->d_name,"idhost")) continue;
    if (str_equal(d->d_name,"localiphost")) continue;
    if (str_equal(d->d_name,"locals")) continue;
    if (str_equal(d->d_name,"me")) continue;
    if (str_equal(d->d_name,"morercpthosts")) continue;
    if (str_equal(d->d_name,"morercpthosts.cdb")) continue;
    if (str_equal(d->d_name,"percenthack")) continue;
    if (str_equal(d->d_name,"plusdomain")) continue;
    if (str_equal(d->d_name,"qmqpservers")) continue;
    if (str_equal(d->d_name,"queuelifetime")) continue;
    if (str_equal(d->d_name,"rcpthosts")) continue;
    if (str_equal(d->d_name,"smtpgreeting")) continue;
    if (str_equal(d->d_name,"smtproutes")) continue;
    if (str_equal(d->d_name,"timeoutconnect")) continue;
    if (str_equal(d->d_name,"timeoutremote")) continue;
    if (str_equal(d->d_name,"timeoutsmtpd")) continue;
    if (str_equal(d->d_name,"virtualdomains")) continue;
    buffer_puts(buffer_1,"\n");
    buffer_puts(buffer_1,d->d_name);
    buffer_puts(buffer_1,": I have no idea what this file does.\n");
  }

  buffer_flush(buffer_1);
  _exit(0);
}
