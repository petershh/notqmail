#include <errno.h>

#include <unistd.h>
#include <signal.h>

#include <skalibs/sig.h>
#include <skalibs/stralloc.h>
#include <skalibs/buffer.h>
#include <skalibs/alloc.h>
#include <skalibs/bytestr.h>
#include <skalibs/types.h>
#include <skalibs/djbunix.h>
#include <skalibs/tai.h>
#include <skalibs/unix-timed.h>

/*
#include "fd.h"
#include "sig.h"
#include "stralloc.h"
#include "substdio.h"
#include "alloc.h"
#include "str.h"
#include "byte.h"
#include "now.h"
#include "fmt.h"
#include "exit.h"
#include "readwrite.h"
#include "timeoutread.h"
#include "timeoutwrite.h"
*/

#include "wait.h"
#include "commands.h"
#include "noreturn.h"

void _noreturn_ die() { _exit(1); }

/*
GEN_SAFE_TIMEOUTREAD(saferead,1200,fd,die())
GEN_SAFE_TIMEOUTWRITE(safewrite,1200,fd,die())
*/

char boutbuf[128];
buffer bout = BUFFER_INIT(buffer_write,1,boutbuf,sizeof(boutbuf));

char binbuf[128];
buffer bin = BUFFER_INIT(buffer_read,0,binbuf,sizeof(binbuf));

void puts(char *s)
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
  puts("-ERR ");
  puts(s);
  puts("\r\n");
  flush();
}

void _noreturn_ die_usage(void)
{
  err("usage: popup hostname subprogram"); die();
}

void _noreturn_ die_nomem(void) { err("out of memory"); die(); }
void _noreturn_ die_pipe(void) { err("unable to open pipe"); die(); }
void _noreturn_ die_write(void) { err("unable to write pipe"); die(); }
void _noreturn_ die_fork(void) { err("unable to fork"); die(); }
void die_childcrashed(void) { err("aack, child crashed"); }
void die_badauth(void) { err("authorization failed"); }

void err_syntax(void) { err("syntax error"); }
void err_wantuser(void) { err("USER first"); }
void err_authoriz(char *arg) { err("authorization first"); }

void okay(char *arg) { puts("+OK \r\n"); flush(); }
void _noreturn_ pop3_quit(char *arg) { okay(0); die(); }


char unique[ULONG_FMT + ULONG_FMT + 3];
char *hostname;
stralloc username = STRALLOC_ZERO;
int seenuser = 0;
char **childargs;
buffer bup;
char upbuf[128];

void _noreturn_ doanddie(char *user,
                         unsigned int userlen, /* including 0 byte */
                         char *pass)
{
  int child;
  int wstat;
  int pi[2];
 
  close(3);
  if (pipe(pi) == -1) die_pipe();
  if (pi[0] != 3) die_pipe();
  switch(child = fork()) {
    case -1:
      die_fork();
    case 0:
      close(pi[1]);
      sig_restore(SIGPIPE);
      execvp(*childargs,childargs);
      _exit(1);
  }
  close(pi[0]);
  buffer_init(&bup,buffer_write,pi[1],upbuf,sizeof(upbuf));
  if (buffer_put(&bup,user,userlen) == -1) die_write();
  if (buffer_put(&bup,pass,str_len(pass) + 1) == -1) die_write();
  if (buffer_puts(&bup,"<") == -1) die_write();
  if (buffer_puts(&bup,unique) == -1) die_write();
  if (buffer_puts(&bup,hostname) == -1) die_write();
  if (buffer_put(&bup,">",2) == -1) die_write();
  if (buffer_flush(&bup) == -1) die_write();
  close(pi[1]);
  byte_zero(pass,str_len(pass));
  byte_zero(upbuf,sizeof(upbuf));
  if (wait_pid(child, &wstat) == -1) die();
  if (wait_crashed(wstat)) die_childcrashed();
  if (wait_exitcode(wstat)) die_badauth();
  die();
}
void pop3_greet(void)
{
  tai now;
  char *s;
  s = unique;
  s += pid_fmt(s,getpid());
  *s++ = '.';
  tai_now(&now);
  s += ulong_fmt(s, (unsigned long) tai_sec(&now));
  *s++ = '@';
  *s++ = 0;
  puts("+OK <");
  puts(unique);
  puts(hostname);
  puts(">\r\n");
  flush();
}
void pop3_user(char *arg)
{
  if (!*arg) { err_syntax(); return; }
  okay(0);
  seenuser = 1;
  if (!stralloc_copys(&username,arg)) die_nomem(); 
  if (!stralloc_0(&username)) die_nomem(); 
}
void pop3_pass(char *arg)
{
  if (!seenuser) { err_wantuser(); return; }
  if (!*arg) { err_syntax(); return; }
  doanddie(username.s,username.len,arg);
}
void pop3_apop(char *arg)
{
  char *space;
  space = arg + str_chr(arg,' ');
  if (!*space) { err_syntax(); return; }
  *space++ = 0;
  doanddie(arg,space - arg,space);
}

struct commands pop3commands[] = {
  { "user", pop3_user, 0 }
, { "pass", pop3_pass, 0 }
, { "apop", pop3_apop, 0 }
, { "quit", pop3_quit, 0 }
, { "noop", okay, 0 }
, { 0, err_authoriz, 0 }
} ;

int main(int argc, char **argv)
{
  sig_catch(SIGALRM, die);
  sig_ignore(SIGPIPE);
 
  hostname = argv[1];
  if (!hostname) die_usage();
  childargs = argv + 2;
  if (!*childargs) die_usage();
 
  pop3_greet();
  commands(&bin,pop3commands);
  die();
}
