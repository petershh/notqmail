#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#include <skalibs/sig.h>
#include <skalibs/types.h>
#include <skalibs/alloc.h>
#include <skalibs/buffer.h>
#include <skalibs/djbunix.h>
#include <skalibs/tai.h>
#include <skalibs/djbtime.h>
#include <skalibs/bytestr.h>

/*
#include "readwrite.h"
#include "sig.h"
#include "exit.h"
#include "open.h"
#include "seek.h"
#include "alloc.h"
#include "substdio.h"
#include "datetime.h"
#include "now.h"
*/

#include "substdio.h"
#include "fmt.h"
#include "triggerpull.h"
#include "extra.h"
#include "noreturn.h"
#include "uidgid.h"
#include "auto_qmail.h"
#include "auto_uids.h"
#include "auto_users.h"
#include "date822fmt.h"
#include "fmtqfn.h"

#define DEATH 86400 /* 24 hours; _must_ be below q-s's OSSIFIED (36 hours) */
#define ADDR 1003

#define PIDFMT_LEN 4 + ULONG_FMT + 1 + TAIN_FMT + 1 + ULONG_FMT + 1
#define RECIEVED_FMT 17 + PID_FMT + 9 + 7 + PID_FMT + 3 + DATE822FMT + 1

char inbuf[2048];
buffer bin;
char outbuf[256];
buffer bout;

tain starttime;
struct tm dt;
unsigned long mypid;
uid_t uid;
char *pidfn;
struct stat pidst;
unsigned long messnum;
char *messfn;
char *todofn;
char *intdfn;
int messfd;
int intdfd;
int flagmademess = 0;
int flagmadeintd = 0;

uid_t auto_uida;
uid_t auto_uidd;
uid_t auto_uids;

void cleanup(void)
{
 if (flagmadeintd)
  {
   ftruncate(intdfd,0);
   if (unlink(intdfn) == -1) return;
  }
 if (flagmademess)
  {
   ftruncate(messfd,0);
   if (unlink(messfn) == -1) return;
  }
}

void _noreturn_ die(int e) { _exit(e); }
void _noreturn_ die_write() { cleanup(); die(53); }
void _noreturn_ die_read() { cleanup(); die(54); }
void _noreturn_ sigalrm() { /* thou shalt not clean up here */ die(52); }
void _noreturn_ sigbug() { die(81); }

unsigned int receivedlen;
char *received;
/* "Received: (qmail-queue invoked by alias); 26 Sep 1995 04:46:54 -0000\n" */

static unsigned int receivedfmt(char *s)
{
 unsigned int i;
 unsigned int len;
 len = 0;
 i = fmt_str(s,"Received: (qmail "); len += i; if (s) s += i;
 i = pid_fmt(s,mypid); len += i; if (s) s += i;
 i = fmt_str(s," invoked "); len += i; if (s) s += i;
 if (uid == auto_uida)
  { i = fmt_str(s,"by alias"); len += i; if (s) s += i; }
 else if (uid == auto_uidd)
  { i = fmt_str(s,"from network"); len += i; if (s) s += i; }
 else if (uid == auto_uids)
  { i = fmt_str(s,"for bounce"); len += i; if (s) s += i; }
 else
  {
   i = fmt_str(s,"by uid "); len += i; if (s) s += i;
   i = pid_fmt(s,uid); len += i; if (s) s += i;
  }
 i = fmt_str(s,"); "); len += i; if (s) s += i;
 i = date822fmt(s,&dt); len += i; if (s) s += i;
 return len;
}

void received_setup(void)
{
 received = alloc(RECIEVED_FMT);
 if (!received) die(51);
 receivedfmt(received);
}

unsigned int pidfmt(char *s, unsigned long seq)
{
 unsigned int i;
 unsigned int len;

 len = 0;
 i = fmt_str(s,"pid/"); len += i; if (s) s += i;
 i = ulong_fmt(s,mypid); len += i; if (s) s += i;
 i = fmt_str(s,"."); len += i; if (s) s += i;
 i = tain_fmt(s,&starttime); len += i; if (s) s += i;
 i = fmt_str(s,"."); len += i; if (s) s += i;
 i = ulong_fmt(s,seq); len += i; if (s) s += i;
 ++len; if (s) *s++ = 0;

 return len;
}

char *fnnum(char *dirslash, int flagsplit)
{
 char *s;

 s = alloc(FMTQFN_MIN + str_len(dirslash));
 if (!s) die(51);
 fmtqfn(s,dirslash,messnum,flagsplit);
 return s;
}

void pidopen(void)
{
 unsigned int len;
 unsigned long seq;

 seq = 1;
 pidfn = alloc(PID_FMT);
 if (!pidfn) die(51);

 for (seq = 1;seq < 10;++seq)
  {
   pidfmt(pidfn,seq);
   messfd = open_excl(pidfn);
   if (messfd != -1) return;
  }

 die(63);
}

char tmp[ULONG_FMT];

int main(void)
{
 unsigned int len;
 char ch;

 sig_blocknone();
 umask(033);
 if (chdir(auto_qmail) == -1) die(61);
 if (chdir("queue") == -1) die(62);

 mypid = getpid();
 uid = getuid();
 tain_now(&starttime);
 localtm_from_tai(&dt, tain_secp(&starttime), 0);

 auto_uida = inituid(auto_usera);
 auto_uidd = inituid(auto_userd);
 auto_uids = inituid(auto_users);

 received_setup();

 sig_ignore(SIGPIPE);

 sig_ignore(SIGVTALRM);
 sig_ignore(SIGPROF);
 sig_ignore(SIGQUIT);
 sig_ignore(SIGINT);
 sig_ignore(SIGHUP);
#ifdef SIGXCPU
  sig_ignore(SIGXCPU);
#endif
#ifdef SIGXFSZ
 sig_ignore(SIGXFSZ);
#endif
 sig_catch(SIGALRM, sigalrm);

 sig_catch(SIGILL, sigbug);
 sig_catch(SIGABRT, sigbug);
 sig_catch(SIGFPE, sigbug);
 sig_catch(SIGBUS, sigbug);
 sig_catch(SIGSEGV, sigbug);
#ifdef SIGSYS
 sig_catch(SIGSYS, sigbug);
#endif
#ifdef SIGEMT
 sig_catch(SIGEMT, sigbug);
#endif

 alarm(DEATH);

 pidopen();
 if (fstat(messfd,&pidst) == -1) die(63);

 messnum = pidst.st_ino;
 messfn = fnnum("mess/",1);
 todofn = fnnum("todo/",0);
 intdfn = fnnum("intd/",0);

 if (link(pidfn,messfn) == -1) die(64);
 if (unlink(pidfn) == -1) die(63);
 flagmademess = 1;

 buffer_init(&bout,buffer_write,messfd,outbuf,sizeof(outbuf));
 buffer_init(&bin,buffer_read,0,inbuf,sizeof(inbuf));

 if (buffer_put(&bout,received,receivedlen) == -1) die_write();

 switch(buffer_copy(&bout,&bin))
  {
   case -2: die_read();
   case -3: die_write();
  }

 if (buffer_flush(&bout) == -1) die_write();
 if (fsync(messfd) == -1) die_write();

 intdfd = open_excl(intdfn);
 if (intdfd == -1) die(65);
 flagmadeintd = 1;

 buffer_init(&bout,buffer_write,intdfd,outbuf,sizeof(outbuf));
 buffer_init(&bin,buffer_read,1,inbuf,sizeof(inbuf));

 if (buffer_put(&bout,"u",1) == -1) die_write();
 if (buffer_put(&bout,tmp,ulong_fmt(tmp,uid)) == -1) die_write();
 if (buffer_put(&bout,"",1) == -1) die_write();

 if (buffer_put(&bout,"p",1) == -1) die_write();
 if (buffer_put(&bout,tmp,ulong_fmt(tmp,mypid)) == -1) die_write();
 if (buffer_put(&bout,"",1) == -1) die_write();

 if (buffer_get(&bin,&ch,1) < 1) die_read();
 if (ch != 'F') die(91);
 if (buffer_put(&bout,&ch,1) == -1) die_write();
 for (len = 0;len < ADDR;++len)
  {
   if (buffer_get(&bin,&ch,1) < 1) die_read();
   if (buffer_put(&bout,&ch,1) == -1) die_write();
   if (!ch) break;
  }
 if (len >= ADDR) die(11);

 if (buffer_put(&bout,QUEUE_EXTRA,QUEUE_EXTRALEN) == -1) die_write();

 for (;;)
  {
   if (buffer_get(&bin,&ch,1) < 1) die_read();
   if (!ch) break;
   if (ch != 'T') die(91);
   if (buffer_put(&bout,&ch,1) == -1) die_write();
   for (len = 0;len < ADDR;++len)
    {
     if (buffer_get(&bin,&ch,1) < 1) die_read();
     if (buffer_put(&bout,&ch,1) == -1) die_write();
     if (!ch) break;
    }
   if (len >= ADDR) die(11);
  }

 if (buffer_flush(&bout) == -1) die_write();
 if (fsync(intdfd) == -1) die_write();

 if (link(intdfn,todofn) == -1) die(66);

 triggerpull();
 return 0;
}
