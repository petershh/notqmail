#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <signal.h>

#include <skalibs/sig.h>
#include <skalibs/stralloc.h>
#include <skalibs/buffer.h>
#include <skalibs/alloc.h>
#include <skalibs/types.h>
#include <skalibs/djbunix.h>
#include <skalibs/tai.h>
#include <skalibs/genalloc.h>
#include <skalibs/bytestr.h>
#include <skalibs/unix-timed.h>

#include "commands.h"
#include "getln.h"
#include "prioq.h"
#include "maildir.h"

void die() { _exit(0); }

/*
GEN_SAFE_TIMEOUTREAD(saferead,1200,fd,die())
GEN_SAFE_TIMEOUTWRITE(safewrite,1200,fd,die())
*/

char berrbuf[128]; /* TODO does it need to be used with unix-timed? */
buffer berr = BUFFER_INIT(buffer_write,2,berrbuf,sizeof(berrbuf));

char boutbuf[1024];
buffer bout = BUFFER_INIT(buffer_write,1,boutbuf,sizeof(boutbuf));

char binbuf[128];
buffer bin = BUFFER_INIT(buffer_read,0,binbuf,sizeof(binbuf));

void put(char *buf, int len)
{
  tain now, deadline;
  tain_now(&now);
  tain_addsec(&deadline, &now, 1200);
  if (buffer_timed_put(&bout, buf, len, &deadline, &now) <= 0)
    die();
}
void timed_puts(char *s)
{
  tain now, deadline;
  tain_now(&now);
  tain_addsec(&deadline, &now, 1200);
  if (buffer_timed_puts(&bout, s, &deadline, &now) <= 0)
    die();
}
void flush(void)
{
  tain now, deadline;
  tain_now(&now);
  tain_addsec(&deadline, &now, 1200);
  if (buffer_timed_flush(&bout, &deadline, &now) <= 0)
    die();
}
void err(char *s)
{
  timed_puts("-ERR ");
  timed_puts(s);
  timed_puts("\r\n");
  flush();
}

void die_nomem(void) { err("out of memory"); die(); }
void die_nomaildir(void) { err("this user has no $HOME/Maildir"); die(); }

void die_root(void)
{
  tain now, deadline;
  tain_now(&now);
  tain_addsec(&deadline, &now, 1200);
  buffer_timed_puts(&berr, "qmail-pop3d invoked as uid 0, terminating\n",
      &deadline, &now);
  buffer_timed_flush(&berr, &deadline, &now);
  _exit(1);
}

void die_scan(void) { err("unable to scan $HOME/Maildir"); die(); }

void err_syntax(void) { err("syntax error"); }
void err_unimpl(char *arg) { err("unimplemented"); }
void err_deleted(void) { err("already deleted"); }
void err_nozero(void) { err("messages are counted from 1"); }
void err_toobig(void) { err("not that many messages"); }
void err_nosuch(void) { err("unable to open that message"); }
void err_nounlink(void) { err("unable to unlink all deleted messages"); }

void okay(char *arg) { timed_puts("+OK \r\n"); flush(); }

void printfn(char *fn)
{
  fn += 4;
  put(fn,str_chr(fn,':'));
}

char strnum[ULONG_FMT];
stralloc line = STRALLOC_ZERO;

void blast(buffer *bfrom, unsigned long limit)
{
  int match;
  int inheaders = 1;
 
  for (;;) {
    if (getln(bfrom,&line,&match,'\n') != 0) die();
    if (!match && !line.len) break;
    if (match) --line.len; /* no way to pass this info over POP */
    if (limit) if (!inheaders) if (!--limit) break;
    if (!line.len)
      inheaders = 0;
    else
      if (line.s[0] == '.')
        put(".",1);
    put(line.s,line.len);
    put("\r\n",2);
    if (!match) break;
  }
  put("\r\n.\r\n",5);
  flush();
}

stralloc filenames = STRALLOC_ZERO;
genalloc pq = GENALLOC_ZERO;

struct message {
  int flagdeleted;
  unsigned long size;
  char *fn;
} *m;
unsigned int numm;

int last = 0;

void getlist(void)
{
  struct prioq_elt pe;
  struct stat st;
  unsigned int i;
 
  maildir_clean(&line);
  if (maildir_scan(&pq,&filenames,1,1) == -1) die_scan();
 
  numm = genalloc_s(struct prioq_elt, &pq) ?
      genalloc_len(struct prioq_elt, &pq) : 0;
  m = calloc(numm, sizeof(struct message));
  if (!m) die_nomem();
 
  for (i = 0;i < numm;++i) {
    if (!prioq_min(&pq,&pe)) { numm = i; break; }
    prioq_delmin(&pq);
    m[i].fn = filenames.s + pe.id;
    m[i].flagdeleted = 0;
    if (stat(m[i].fn,&st) == -1)
      m[i].size = 0;
    else
      m[i].size = st.st_size;
  }
}

void pop3_stat(char *arg)
{
  unsigned int i;
  unsigned long total;
 
  total = 0;
  for (i = 0;i < numm;++i) if (!m[i].flagdeleted) total += m[i].size;
  timed_puts("+OK ");
  put(strnum,uint_fmt(strnum,numm));
  timed_puts(" ");
  put(strnum,ulong_fmt(strnum,total));
  timed_puts("\r\n");
  flush();
}

void pop3_rset(char *arg)
{
  unsigned int i;
  for (i = 0;i < numm;++i) m[i].flagdeleted = 0;
  last = 0;
  okay(0);
}

void pop3_last(char *arg)
{
  timed_puts("+OK ");
  put(strnum,uint_fmt(strnum,last));
  timed_puts("\r\n");
  flush();
}

void pop3_quit(char *arg)
{
  unsigned int i;
  for (i = 0;i < numm;++i)
    if (m[i].flagdeleted) {
      if (unlink(m[i].fn) == -1) err_nounlink();
    }
    else
      if (str_start(m[i].fn,"new/")) {
        if (!stralloc_copys(&line,"cur/")) die_nomem();
        if (!stralloc_cats(&line,m[i].fn + 4)) die_nomem();
        if (!stralloc_cats(&line,":2,")) die_nomem();
        if (!stralloc_0(&line)) die_nomem();
        rename(m[i].fn,line.s); /* if it fails, bummer */
      }
  okay(0);
  die();
}

int msgno(char *arg)
{
  unsigned long u;
  if (!ulong_scan(arg,&u)) { err_syntax(); return -1; }
  if (!u) { err_nozero(); return -1; }
  --u;
  if (u >= numm || u >= INT_MAX) { err_toobig(); return -1; }
  if (m[u].flagdeleted) { err_deleted(); return -1; }
  return u;
}

void pop3_dele(char *arg)
{
  int i;
  i = msgno(arg);
  if (i == -1) return;
  m[i].flagdeleted = 1;
  if (i + 1 > last) last = i + 1;
  okay(0);
}

void list(int i, int flaguidl)
{
  put(strnum,uint_fmt(strnum,i + 1));
  timed_puts(" ");
  if (flaguidl) printfn(m[i].fn);
  else put(strnum,ulong_fmt(strnum,m[i].size));
  timed_puts("\r\n");
}

void dolisting(char *arg, int flaguidl)
{
  unsigned int i;
  if (*arg) {
    i = msgno(arg);
    if (i == -1) return;
    timed_puts("+OK ");
    list(i,flaguidl);
  }
  else {
    okay(0);
    for (i = 0;i < numm;++i)
      if (!m[i].flagdeleted)
        list(i,flaguidl);
    timed_puts(".\r\n");
  }
  flush();
}

void pop3_uidl(char *arg) { dolisting(arg,1); }
void pop3_list(char *arg) { dolisting(arg,0); }

buffer bmsg; char bmsgbuf[1024];

void pop3_top(char *arg)
{
  int i;
  unsigned long limit;
  int fd;
 
  i = msgno(arg);
  if (i == -1) return;
 
  arg += ulong_scan(arg,&limit);
  while (*arg == ' ') ++arg;
  if (ulong_scan(arg,&limit)) ++limit; else limit = 0;
 
  fd = open_read(m[i].fn);
  if (fd == -1) { err_nosuch(); return; }
  okay(0);
  buffer_init(&bmsg, buffer_read, fd, bmsgbuf, sizeof(bmsgbuf));
  blast(&bmsg,limit);
  close(fd);
}

struct commands pop3commands[] = {
  { "quit", pop3_quit, 0 }
, { "stat", pop3_stat, 0 }
, { "list", pop3_list, 0 }
, { "uidl", pop3_uidl, 0 }
, { "dele", pop3_dele, 0 }
, { "retr", pop3_top, 0 }
, { "rset", pop3_rset, 0 }
, { "last", pop3_last, 0 }
, { "top", pop3_top, 0 }
, { "noop", okay, 0 }
, { 0, err_unimpl, 0 }
} ;

int main(int argc, char **argv)
{
  sig_catch(SIGALRM, die);
  sig_ignore(SIGPIPE);
 
  if (!getuid()) die_root();
  if (!argv[1]) die_nomaildir();
  if (chdir(argv[1]) == -1) die_nomaildir();
 
  getlist();

  okay(0);
  commands(&bin,pop3commands);
  die();
}
