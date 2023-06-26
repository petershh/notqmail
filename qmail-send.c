#include <errno.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <signal.h>

#include <skalibs/sig.h>
#include <skalibs/direntry.h>
#include <skalibs/iopause.h>
#include <skalibs/djbunix.h>
#include <skalibs/buffer.h>
#include <skalibs/alloc.h>
#include <skalibs/stralloc.h>
#include <skalibs/bytestr.h>
#include <skalibs/types.h>
#include <skalibs/allreadwrite.h>
#include <skalibs/tai.h>
#include <skalibs/genalloc.h>

/*
#include "readwrite.h"
#include "sig.h"
#include "direntry.h"
#include "select.h"
#include "open.h"
#include "seek.h"
#include "exit.h"
#include "ndelay.h"
#include "substdio.h"
#include "alloc.h"
#include "error.h"
#include "stralloc.h"
#include "str.h"
#include "byte.h"
#include "fmt.h"
#include "scan.h"
#include "case.h"
#include "now.h"
*/

#include "control.h"
#include "lock.h"
#include "getln.h"
#include "auto_qmail.h"
#include "trigger.h"
#include "newfield.h"
#include "quote.h"
#include "qmail.h"
#include "qsutil.h"
#include "prioq.h"
#include "constmap.h"
#include "fmtqfn.h"
#include "readsubdir.h"


/* critical timing feature #1: if not triggered, do not busy-loop */
/* critical timing feature #2: if triggered, respond within fixed time */
/* important timing feature: when triggered, respond instantly */
#define SLEEP_TODO 1500 /* check todo/ every 25 minutes in any case */
#define SLEEP_FUZZ 1 /* slop a bit on sleeps to avoid zeno effect */
#define SLEEP_FOREVER 86400 /* absolute maximum time spent in select() */
#define SLEEP_CLEANUP 76431 /* time between cleanups */
#define SLEEP_SYSFAIL 123
#define OSSIFIED 129600 /* 36 hours; _must_ exceed q-q's DEATH (24 hours) */

int lifetime = 604800;

stralloc percenthack = STRALLOC_ZERO;
struct constmap mappercenthack;
stralloc locals = STRALLOC_ZERO;
struct constmap maplocals;
stralloc vdoms = STRALLOC_ZERO;
struct constmap mapvdoms;
stralloc envnoathost = STRALLOC_ZERO;
stralloc bouncefrom = STRALLOC_ZERO;
stralloc bouncehost = STRALLOC_ZERO;
stralloc doublebounceto = STRALLOC_ZERO;
stralloc doublebouncehost = STRALLOC_ZERO;

char strnum2[ULONG_FMT];
char strnum3[ULONG_FMT];

#define CHANNELS 2
char *chanaddr[CHANNELS] = { "local/", "remote/" };
char *chanstatusmsg[CHANNELS] = { " local ", " remote " };
char *tochan[CHANNELS] = { " to local ", " to remote " };
int chanfdout[CHANNELS] = { 1, 3 };
int chanfdin[CHANNELS] = { 2, 4 };
int chanskip[CHANNELS] = { 10, 20 };

/* XXX: this is blatantly incorrect signal handling
 * DJB, you invented self-pipes for this! WTF? */
int flagexitasap = 0; void sigterm(int sig) { flagexitasap = 1; }
int flagrunasap = 0; void sigalrm(int sig) { flagrunasap = 1; }
int flagreadasap = 0; void sighup(int sig) { flagreadasap = 1; }

void cleandied() { log1("alert: oh no! lost qmail-clean connection! dying...\n");
 flagexitasap = 1; }

int flagspawnalive[CHANNELS];
void spawndied(c) int c; { log1("alert: oh no! lost spawn connection! dying...\n");
 flagspawnalive[c] = 0; flagexitasap = 1; }

#define REPORTMAX 10000

tain recent;


/* this file is too long ----------------------------------------- FILENAMES */

stralloc fn = STRALLOC_ZERO;
stralloc fn2 = STRALLOC_ZERO;
char fnmake_strnum[ULONG_FMT];

void fnmake_init(void)
{
 while (!stralloc_ready(&fn,FMTQFN)) nomem();
 while (!stralloc_ready(&fn2,FMTQFN)) nomem();
}

void fnmake_info(unsigned long id) { fn.len = fmtqfn(fn.s,"info/",id,1); }
void fnmake_todo(unsigned long id) { fn.len = fmtqfn(fn.s,"todo/",id,0); }
void fnmake_mess(unsigned long id) { fn.len = fmtqfn(fn.s,"mess/",id,1); }
void fnmake_foop(unsigned long id) { fn.len = fmtqfn(fn.s,"foop/",id,0); }
void fnmake_split(unsigned long id) { fn.len = fmtqfn(fn.s,"",id,1); }
void fnmake2_bounce(unsigned long id)
{ fn2.len = fmtqfn(fn2.s,"bounce/",id,0); }
void fnmake_chanaddr(unsigned id, int c)
{ fn.len = fmtqfn(fn.s,chanaddr[c],id,1); }


/* this file is too long ----------------------------------------- REWRITING */

stralloc rwline = STRALLOC_ZERO;

/* 1 if by land, 2 if by sea, 0 if out of memory. not allowed to barf. */
/* may trash recip. must set up rwline, between a T and a \0. */
int rewrite(recip)
char *recip;
{
  unsigned int i;
  char *x;
  static stralloc addr = STRALLOC_ZERO;
  unsigned int at;

  if (!stralloc_copys(&rwline,"T")) return 0;
  if (!stralloc_copys(&addr,recip)) return 0;

  i = byte_rchr(addr.s,addr.len,'@');
  if (i == addr.len) {
    if (!stralloc_cats(&addr,"@")) return 0;
    if (!stralloc_cat(&addr,&envnoathost)) return 0;
  }

  while (constmap(&mappercenthack,addr.s + i + 1,addr.len - i - 1)) {
    unsigned int j = byte_rchr(addr.s,i,'%');
    if (j == i) break;
    addr.len = i;
    i = j;
    addr.s[i] = '@';
  }

  at = byte_rchr(addr.s,addr.len,'@');

  if (constmap(&maplocals,addr.s + at + 1,addr.len - at - 1)) {
    if (!stralloc_cat(&rwline,&addr)) return 0;
    if (!stralloc_0(&rwline)) return 0;
    return 1;
  }

  for (i = 0;i <= addr.len;++i)
    if (!i || (i == at + 1) || (i == addr.len) || ((i > at) && (addr.s[i] == '.')))
      if ((x = constmap(&mapvdoms,addr.s + i,addr.len - i))) {
        if (!*x) break;
        if (!stralloc_cats(&rwline,x)) return 0;
        if (!stralloc_cats(&rwline,"-")) return 0;
        if (!stralloc_cat(&rwline,&addr)) return 0;
        if (!stralloc_0(&rwline)) return 0;
        return 1;
      }
 
  if (!stralloc_cat(&rwline,&addr)) return 0;
  if (!stralloc_0(&rwline)) return 0;
  return 2;
}

void senderadd(stralloc *sa, char *sender, char *recip)
{
 unsigned int i;

 i = str_len(sender);
 if (i >= 4)
   if (str_equal(sender + i - 4,"-@[]"))
    {
     unsigned int j = byte_rchr(sender,i - 4,'@');
     unsigned int k = str_rchr(recip,'@');
     if (recip[k] && (j + 5 <= i))
      {
       /* owner-@host-@[] -> owner-recipbox=reciphost@host */
       while (!stralloc_catb(sa,sender,j)) nomem();
       while (!stralloc_catb(sa,recip,k)) nomem();
       while (!stralloc_cats(sa,"=")) nomem();
       while (!stralloc_cats(sa,recip + k + 1)) nomem();
       while (!stralloc_cats(sa,"@")) nomem();
       while (!stralloc_catb(sa,sender + j + 1,i - 5 - j)) nomem();
       return;
      }
    }
 while (!stralloc_cats(sa,sender)) nomem();
}


/* this file is too long ---------------------------------------------- INFO */

int getinfo(stralloc *sa, tain *dt, unsigned long id)
{
 int fdinfo;
 struct stat st;
 static stralloc line = STRALLOC_ZERO;
 int match;
 buffer b;
 char buf[128];

 fnmake_info(id);
 fdinfo = open_read(fn.s);
 if (fdinfo == -1) return 0;
 if (fstat(fdinfo,&st) == -1) { close(fdinfo); return 0; }
 buffer_init(&b,buffer_read,fdinfo,buf,sizeof(buf));
 if (getln(&b,&line,&match,'\0') == -1) { close(fdinfo); return 0; }
 close(fdinfo);
 if (!match) return 0;
 if (line.s[0] != 'F') return 0;

 tain_from_timespec_sysclock(dt, &st.st_mtim);
 while (!stralloc_copys(sa,line.s + 1)) nomem();
 while (!stralloc_0(sa)) nomem();
 return 1;
}


/* this file is too long ------------------------------------- COMMUNICATION */

buffer btoqc; char btoqcbuf[1024];
buffer bfromqc; char bfromqcbuf[1024];
stralloc comm_buf[CHANNELS] = { STRALLOC_ZERO, STRALLOC_ZERO };
int comm_pos[CHANNELS];

void comm_init(void)
{
 int c;
 buffer_init(&btoqc,buffer_write,5,btoqcbuf,sizeof(btoqcbuf));
 buffer_init(&bfromqc,buffer_read,6,bfromqcbuf,sizeof(bfromqcbuf));
 for (c = 0;c < CHANNELS;++c)
   if (ndelay_on(chanfdout[c]) == -1)
   /* this is so stupid: NDELAY semantics should be default on write */
     spawndied(c); /* drastic, but better than risking deadlock */
}

int comm_canwrite(int c)
{
 /* XXX: could allow a bigger buffer; say 10 recipients */
 if (comm_buf[c].s && comm_buf[c].len) return 0;
 return 1;
}

void comm_write(int c, int delnum, unsigned long id, char *sender, char *recip)
{
 char ch;
 if (comm_buf[c].s && comm_buf[c].len) return;
 while (!stralloc_copys(&comm_buf[c],"")) nomem();
 ch = delnum;
 while (!stralloc_append(&comm_buf[c],ch)) nomem();
 fnmake_split(id);
 while (!stralloc_cats(&comm_buf[c],fn.s)) nomem();
 while (!stralloc_0(&comm_buf[c])) nomem();
 senderadd(&comm_buf[c],sender,recip);
 while (!stralloc_0(&comm_buf[c])) nomem();
 while (!stralloc_cats(&comm_buf[c],recip)) nomem();
 while (!stralloc_0(&comm_buf[c])) nomem();
 comm_pos[c] = 0;
}

void comm_selprep(int *nfds, iopause_fd *wfds)
{
 int c;
 for (c = 0;c < CHANNELS;++c)
   if (flagspawnalive[c])
     if (comm_buf[c].s && comm_buf[c].len)
      {
       wfds[*nfds].fd = chanfdout[c];
       wfds[*nfds].events = IOPAUSE_WRITE;
       *nfds += 1;
      }
}

void comm_do(iopause_fd *wfds, int *j)
{
  int c;
  for (c = 0;c < CHANNELS;++c)
    if (flagspawnalive[c])
      if (comm_buf[c].s && comm_buf[c].len)
        if (wfds[*j].revents | IOPAUSE_WRITE)
        {
          *j += 1;
          int w;
          int len;
          len = comm_buf[c].len;
          w = write(chanfdout[c],comm_buf[c].s + comm_pos[c],len - comm_pos[c]);
          if (w == 0 || w == -1)
          {
            if ((w == -1) && (errno == EPIPE))
              spawndied(c);
            else
              continue; /* kernel select() bug; can't avoid busy-looping */
          }
          else
          {
            comm_pos[c] += w;
            if (comm_pos[c] == len)
              comm_buf[c].len = 0;
          }
        }
}


/* this file is too long ------------------------------------------ CLEANUPS */

int flagcleanup; /* if 1, cleanupdir is initialized and ready */
readsubdir cleanupdir;
tain cleanuptime;

void cleanup_init(void)
{
 flagcleanup = 0;
 tain_now(&cleanuptime);
}

void cleanup_selprep(tain *wakeup)
{
 if (flagcleanup) *wakeup = tain_zero;
 if (tain_less(&cleanuptime, wakeup)) *wakeup = cleanuptime;
}

void cleanup_do(void)
{
 char ch;
 struct stat st;
 unsigned long id;
 tain atime;

 if (!flagcleanup)
  {
   if (tain_less(&recent, &cleanuptime)) return;
   readsubdir_init(&cleanupdir,"mess",pausedir);
   flagcleanup = 1;
  }

 switch(readsubdir_next(&cleanupdir,&id))
  {
   case 1:
     break;
   case 0:
     flagcleanup = 0;
     tain_addsec(&cleanuptime, &recent, SLEEP_CLEANUP);
   default:
     return;
  }

 fnmake_mess(id);
 if (stat(fn.s,&st) == -1) return; /* probably qmail-queue deleted it */
 tain_from_timespec_sysclock(&atime, &st.st_atim);
 tain_addsec(&atime, &atime, OSSIFIED);
 if (!tain_less(&atime, &recent)) return;

 fnmake_info(id);
 if (stat(fn.s,&st) == 0) return;
 if (errno != ENOENT) return;
 fnmake_todo(id);
 if (stat(fn.s,&st) == 0) return;
 if (errno != ENOENT) return;

 fnmake_foop(id);
 if (buffer_putflush(&btoqc,fn.s,fn.len) == -1) { cleandied(); return; }
 if (buffer_get(&bfromqc,&ch,1) != 1) { cleandied(); return; }
 if (ch != '+')
   log3("warning: qmail-clean unable to clean up ",fn.s,"\n");
}


/* this file is too long ----------------------------------- PRIORITY QUEUES */

genalloc pqdone = GENALLOC_ZERO; /* -todo +info; HOPEFULLY -local -remote */
genalloc pqchan[CHANNELS] = { GENALLOC_ZERO, GENALLOC_ZERO };
/* pqchan 0: -todo +info +local ?remote */
/* pqchan 1: -todo +info ?local +remote */
genalloc pqfail = GENALLOC_ZERO; /* stat() failure; has to be pqadded again */

void pqadd(unsigned long id)
{
 struct prioq_elt pe;
 struct prioq_elt pechan[CHANNELS];
 int flagchan[CHANNELS];
 struct stat st;
 int c;

#define CHECKSTAT if (errno != ENOENT) goto fail;

 fnmake_info(id);
 if (stat(fn.s,&st) == -1)
  {
   CHECKSTAT
   return; /* someone yanking our chain */
  }

 fnmake_todo(id);
 if (stat(fn.s,&st) != -1) return; /* look, ma, dad crashed writing info! */
 CHECKSTAT

 for (c = 0;c < CHANNELS;++c)
  {
   fnmake_chanaddr(id,c);
   if (stat(fn.s,&st) == -1) { flagchan[c] = 0; CHECKSTAT }
   else {
     flagchan[c] = 1;
     pechan[c].id = id;
     tain_from_timespec_sysclock(&pechan[c].dt, &st.st_mtim);
   }
  }

 for (c = 0;c < CHANNELS;++c)
   if (flagchan[c])
     while (!prioq_insert(&pqchan[c],&pechan[c])) nomem();

 for (c = 0;c < CHANNELS;++c) if (flagchan[c]) break;
 if (c == CHANNELS)
  {
   pe.id = id; tain_now(&pe.dt);
   while (!prioq_insert(&pqdone,&pe)) nomem();
  }

 return;

 fail:
 log3("warning: unable to stat ",fn.s,"; will try again later\n");
 pe.id = id;
 tain_now(&pe.dt);
 tain_addsec(&pe.dt, &pe.dt, SLEEP_SYSFAIL);
 while (!prioq_insert(&pqfail,&pe)) nomem();
}

void pqstart(void)
{
 readsubdir rs;
 int x;
 unsigned long id;

 readsubdir_init(&rs,"info",pausedir);

 while ((x = readsubdir_next(&rs,&id)))
   if (x > 0)
     pqadd(id);
}

void pqfinish(void)
{
 int c;
 struct prioq_elt pe;
 struct timeval ut[2] = { 0 };

 for (c = 0;c < CHANNELS;++c)
   while (prioq_min(&pqchan[c],&pe))
    {
     prioq_delmin(&pqchan[c]);
     fnmake_chanaddr(pe.id,c);
     timeval_sysclock_from_tain(&ut[0], &pe.dt);
     if (utimes(fn.s,ut) == -1)
       log3("warning: unable to utime ",fn.s,"; message will be retried too soon\n");
    }
}

void pqrun(void)
{
  int c;
  unsigned int i;
  for (c = 0;c < CHANNELS;++c)
    if (genalloc_s(struct prioq_elt, &pqchan[c]))
      if (genalloc_len(struct prioq_elt, &pqchan[c]))
        for (i = 0;i < genalloc_len(struct prioq_elt, &pqchan[c]);++i)
          genalloc_s(struct prioq_elt, &pqchan[c])[i].dt = recent;
}


/* this file is too long ---------------------------------------------- JOBS */

struct job
 {
  int refs; /* if 0, this struct is unused */
  unsigned long id;
  int channel;
  tain retry;
  stralloc sender;
  int numtodo;
  int flaghiteof;
  int flagdying;
 }
;

int numjobs;
struct job *jo;

void job_init(void)
{
 int j;
 while (!(jo = (struct job *) alloc(numjobs * sizeof(struct job)))) nomem();
 for (j = 0;j < numjobs;++j)
  {
   jo[j].refs = 0;
   jo[j].sender.s = 0;
  }
}

int job_avail(void)
{
 int j;
 for (j = 0;j < numjobs;++j) if (!jo[j].refs) return 1;
 return 0;
}

int job_open(unsigned long id, int channel)
{
 int j;
 for (j = 0;j < numjobs;++j) if (!jo[j].refs) break;
 if (j == numjobs) return -1;
 jo[j].refs = 1;
 jo[j].id = id;
 jo[j].channel = channel;
 jo[j].numtodo = 0;
 jo[j].flaghiteof = 0;
 return j;
}

void job_close(int j) {
  struct prioq_elt pe;
  struct stat st;

  if (0 < --jo[j].refs) return;

  pe.id = jo[j].id;
  pe.dt = jo[j].retry;
  if (jo[j].flaghiteof && !jo[j].numtodo) {
    fnmake_chanaddr(jo[j].id,jo[j].channel);
    if (unlink(fn.s) == -1) {
      log3("warning: unable to unlink ",fn.s,"; will try again later\n");
      tain_now(&pe.dt);
      tain_addsec(&pe.dt, &pe.dt, SLEEP_SYSFAIL);
    }
    else
    {
      int c;
      for (c = 0;c < CHANNELS;++c) if (c != jo[j].channel) {
        fnmake_chanaddr(jo[j].id,c);
        if (stat(fn.s,&st) == 0) return; /* more channels going */
        if (errno != ENOENT) {
          log3("warning: unable to stat ",fn.s,"\n");
          break; /* this is the only reason for HOPEFULLY */
        }
      }
      tain_now(&pe.dt);
      while (!prioq_insert(&pqdone,&pe)) nomem();
      return;
    }
  }

  while (!prioq_insert(&pqchan[jo[j].channel],&pe)) nomem();
}


/* this file is too long ------------------------------------------- BOUNCES */

char *stripvdomprepend(char *recip)
{
  unsigned int i;
  char *domain;
  unsigned int domainlen;
  char *prepend;

  i = str_rchr(recip,'@');
  if (!recip[i]) return recip;
  domain = recip + i + 1;
  domainlen = str_len(domain);

  for (i = 0;i <= domainlen;++i)
    if ((i == 0) || (i == domainlen) || (domain[i] == '.'))
      if ((prepend = constmap(&mapvdoms,domain + i,domainlen - i))) {
        if (!*prepend) break;
        i = str_len(prepend);
        if (str_diffn(recip,prepend,i)) break;
        if (recip[i] != '-') break;
        return recip + i + 1;
      }
  return recip;
}

stralloc bouncetext = STRALLOC_ZERO;

void addbounce(unsigned long id, char *recip, char *report)
{
 int fd;
 unsigned int pos;
 int w;
 while (!stralloc_copys(&bouncetext,"<")) nomem();
 while (!stralloc_cats(&bouncetext,stripvdomprepend(recip))) nomem();
 for (pos = 0;pos < bouncetext.len;++pos)
   if (bouncetext.s[pos] == '\n')
     bouncetext.s[pos] = '_';
 while (!stralloc_cats(&bouncetext,">:\n")) nomem();
 while (!stralloc_cats(&bouncetext,report)) nomem();
 if (report[0])
   if (report[str_len(report) - 1] != '\n')
     while (!stralloc_cats(&bouncetext,"\n")) nomem();
 for (pos = bouncetext.len - 2;pos > 0;--pos)
   if (bouncetext.s[pos] == '\n')
     if (bouncetext.s[pos - 1] == '\n')
       bouncetext.s[pos] = '/';
 while (!stralloc_cats(&bouncetext,"\n")) nomem();
 fnmake2_bounce(id);
 for (;;)
  {
   fd = open_append(fn2.s);
   if (fd != -1) break;
   log1("alert: unable to append to bounce message; HELP! sleeping...\n");
   sleep(10);
  }
 pos = 0;
 while (pos < bouncetext.len)
  {
   w = write(fd,bouncetext.s + pos,bouncetext.len - pos);
   if (w == 0 || w == -1)
    {
     log1("alert: unable to append to bounce message; HELP! sleeping...\n");
     sleep(10);
    }
   else
     pos += w;
  }
 close(fd);
}

int injectbounce(unsigned long id)
{
 struct qmail qqt;
 struct stat st;
 char *bouncesender;
 char *bouncerecip;
 int r;
 int fd;
 buffer bread;
 char buf[128];
 char inbuf[128];
 static stralloc sender = STRALLOC_ZERO;
 static stralloc quoted = STRALLOC_ZERO;
 tain birth;
 tai now;
 unsigned long qp;

 if (!getinfo(&sender,&birth,id)) return 0; /* XXX: print warning */

 /* owner-@host-@[] -> owner-@host */
 if (sender.len >= 5)
   if (str_equal(sender.s + sender.len - 5,"-@[]"))
    {
     sender.len -= 4;
     sender.s[sender.len - 1] = 0;
    }

 fnmake2_bounce(id);
 fnmake_mess(id);
 if (stat(fn2.s,&st) == -1)
  {
   if (errno == ENOENT)
     return 1;
   log3("warning: unable to stat ",fn2.s,"\n");
   return 0;
  }
 if (str_equal(sender.s,"#@[]"))
   log3("triple bounce: discarding ",fn2.s,"\n");
 else
  {
   if (qmail_open(&qqt) == -1)
    { log1("warning: unable to start qmail-queue, will try later\n"); return 0; }
   qp = qmail_qp(&qqt);

   if (*sender.s) { bouncesender = ""; bouncerecip = sender.s; }
   else { bouncesender = "#@[]"; bouncerecip = doublebounceto.s; }

   tai_now(&now);
   while (!newfield_datemake(&now)) nomem();
   qmail_put(&qqt,newfield_date.s,newfield_date.len);
   qmail_puts(&qqt,"From: ");
   while (!quote(&quoted,&bouncefrom)) nomem();
   qmail_put(&qqt,quoted.s,quoted.len);
   qmail_puts(&qqt,"@");
   qmail_put(&qqt,bouncehost.s,bouncehost.len);
   qmail_puts(&qqt,"\nTo: ");
   while (!quote2(&quoted,bouncerecip)) nomem();
   qmail_put(&qqt,quoted.s,quoted.len);
   qmail_puts(&qqt,"\n\
Subject: failure notice\n\
\n\
Hi. This is the qmail-send program at ");
   qmail_put(&qqt,bouncehost.s,bouncehost.len);
   qmail_puts(&qqt,*sender.s ? ".\n\
I'm afraid I wasn't able to deliver your message to the following addresses.\n\
This is a permanent error; I've given up. Sorry it didn't work out.\n\
\n\
" : ".\n\
I tried to deliver a bounce message to this address, but the bounce bounced!\n\
\n\
");

   fd = open_read(fn2.s);
   if (fd == -1)
     qmail_fail(&qqt);
   else
    {
     buffer_init(&bread,buffer_read,fd,inbuf,sizeof(inbuf));
     while ((r = buffer_get(&bread,buf,sizeof(buf))) > 0)
       qmail_put(&qqt,buf,r);
     close(fd);
     if (r == -1)
       qmail_fail(&qqt);
    }

   qmail_puts(&qqt,*sender.s ? "--- Below this line is a copy of the message.\n\n" : "--- Below this line is the original bounce.\n\n");
   qmail_puts(&qqt,"Return-Path: <");
   while (!quote2(&quoted,sender.s)) nomem();
   qmail_put(&qqt,quoted.s,quoted.len);
   qmail_puts(&qqt,">\n");

   fd = open_read(fn.s);
   if (fd == -1)
     qmail_fail(&qqt);
   else
    {
     buffer_init(&bread,buffer_read,fd,inbuf,sizeof(inbuf));
     while ((r = buffer_get(&bread,buf,sizeof(buf))) > 0)
       qmail_put(&qqt,buf,r);
     close(fd);
     if (r == -1)
       qmail_fail(&qqt);
    }

   qmail_from(&qqt,bouncesender);
   qmail_to(&qqt,bouncerecip);
   if (*qmail_close(&qqt))
    { log1("warning: trouble injecting bounce message, will try later\n"); return 0; }

   strnum2[ulong_fmt(strnum2,id)] = 0;
   qslog2("bounce msg ",strnum2);
   strnum2[ulong_fmt(strnum2,qp)] = 0;
   log3(" qp ",strnum2,"\n");
  }
 if (unlink(fn2.s) == -1)
  {
   log3("warning: unable to unlink ",fn2.s,"\n");
   return 0;
  }
 return 1;
}


/* this file is too long ---------------------------------------- DELIVERIES */

struct del
 {
  int used;
  int j;
  unsigned long delid;
  off_t mpos;
  stralloc recip;
 }
;

unsigned long masterdelid = 1;
unsigned int concurrency[CHANNELS] = { 10, 20 };
unsigned int concurrencyused[CHANNELS] = { 0, 0 };
struct del *d[CHANNELS];
stralloc dline[CHANNELS];
char delbuf[2048];

void del_status(void)
{
  int c;

  log1("status:");
  for (c = 0;c < CHANNELS;++c) {
    strnum2[ulong_fmt(strnum2,(unsigned long) concurrencyused[c])] = 0;
    strnum3[ulong_fmt(strnum3,(unsigned long) concurrency[c])] = 0;
    qslog2(chanstatusmsg[c],strnum2);
    qslog2("/",strnum3);
  }
  if (flagexitasap) log1(" exitasap");
  log1("\n");
}

void del_init(void)
{
 int c;
 unsigned int i;
 for (c = 0;c < CHANNELS;++c)
  {
   flagspawnalive[c] = 1;
   while (!(d[c] = (struct del *) alloc(concurrency[c] * sizeof(struct del))))
     nomem();
   for (i = 0;i < concurrency[c];++i)
    { d[c][i].used = 0; d[c][i].recip.s = 0; }
   dline[c].s = 0;
   while (!stralloc_copys(&dline[c],"")) nomem();
  }
 del_status();
}

int del_canexit(void)
{
 int c;
 for (c = 0;c < CHANNELS;++c)
   if (flagspawnalive[c]) /* if dead, nothing we can do about its jobs */
     if (concurrencyused[c]) return 0;
 return 1;
}

int del_avail(int c)
{
  return flagspawnalive[c] && comm_canwrite(c) && (concurrencyused[c] < concurrency[c]);
}

void del_start(int j, off_t mpos, char *recip)
{
 unsigned int i;
 int c;

 c = jo[j].channel;
 if (!flagspawnalive[c]) return;
 if (!comm_canwrite(c)) return;

 for (i = 0;i < concurrency[c];++i) if (!d[c][i].used) break;
 if (i == concurrency[c]) return;

 if (!stralloc_copys(&d[c][i].recip,recip)) { nomem(); return; }
 if (!stralloc_0(&d[c][i].recip)) { nomem(); return; }
 d[c][i].j = j; ++jo[j].refs;
 d[c][i].delid = masterdelid++;
 d[c][i].mpos = mpos;
 d[c][i].used = 1; ++concurrencyused[c];

 comm_write(c,i,jo[j].id,jo[j].sender.s,recip);

 strnum2[ulong_fmt(strnum2,d[c][i].delid)] = 0;
 strnum3[ulong_fmt(strnum3,jo[j].id)] = 0;
 qslog2("starting delivery ",strnum2);
 log3(": msg ",strnum3,tochan[c]);
 logsafe(recip);
 log1("\n");
 del_status();
}

void markdone(int c, unsigned long id, off_t pos)
{
 struct stat st;
 int fd;
 fnmake_chanaddr(id,c);
 for (;;)
  {
   fd = open_write(fn.s);
   if (fd == -1) break;
   if (fstat(fd,&st) == -1) { close(fd); break; }
   if (lseek(fd, pos, SEEK_SET) == -1) { close(fd); break; }
   if (write(fd,"D",1) != 1) { close(fd); break; }
   /* further errors -> double delivery without us knowing about it, oh well */
   close(fd);
   return;
  }
 log3("warning: trouble marking ",fn.s,"; message will be delivered twice!\n");
}

void del_dochan(int c)
{
 int r;
 char ch;
 int i;
 int delnum;
 r = read(chanfdin[c],delbuf,sizeof(delbuf));
 if (r == -1) return;
 if (r == 0) { spawndied(c); return; }
 for (i = 0;i < r;++i)
  {
   ch = delbuf[i];
   while (!stralloc_append(&dline[c],ch)) nomem();
   if (dline[c].len > REPORTMAX)
     dline[c].len = REPORTMAX;
     /* qmail-lspawn and qmail-rspawn are responsible for keeping it short */
     /* but from a security point of view, we don't trust rspawn */
   if (!ch && (dline[c].len > 1))
    {
     delnum = (unsigned int) (unsigned char) dline[c].s[0];
     if ((delnum < 0) || (delnum >= concurrency[c]) || !d[c][delnum].used)
       log1("warning: internal error: delivery report out of range\n");
     else
      {
       strnum3[ulong_fmt(strnum3,d[c][delnum].delid)] = 0;
       if (dline[c].s[1] == 'Z')
	 if (jo[d[c][delnum].j].flagdying)
	  {
	   dline[c].s[1] = 'D';
	   --dline[c].len;
	   while (!stralloc_cats(&dline[c],"I'm not going to try again; this message has been in the queue too long.\n")) nomem();
	   while (!stralloc_0(&dline[c])) nomem();
	  }
       switch(dline[c].s[1])
	{
	 case 'K':
	   log3("delivery ",strnum3,": success: ");
	   logsafe(dline[c].s + 2);
	   log1("\n");
	   markdone(c,jo[d[c][delnum].j].id,d[c][delnum].mpos);
	   --jo[d[c][delnum].j].numtodo;
	   break;
	 case 'Z':
	   log3("delivery ",strnum3,": deferral: ");
	   logsafe(dline[c].s + 2);
	   log1("\n");
	   break;
	 case 'D':
	   log3("delivery ",strnum3,": failure: ");
	   logsafe(dline[c].s + 2);
	   log1("\n");
	   addbounce(jo[d[c][delnum].j].id,d[c][delnum].recip.s,dline[c].s + 2);
	   markdone(c,jo[d[c][delnum].j].id,d[c][delnum].mpos);
	   --jo[d[c][delnum].j].numtodo;
	   break;
	 default:
	   log3("delivery ",strnum3,": report mangled, will defer\n");
	}
       job_close(d[c][delnum].j);
       d[c][delnum].used = 0; --concurrencyused[c];
       del_status();
      }
     dline[c].len = 0;
    }
  }
}

void del_selprep(int *nfds, iopause_fd *rfds)
{
 int c;
 for (c = 0;c < CHANNELS;++c)
   if (flagspawnalive[c])
    {
     rfds[*nfds].fd = chanfdin[c];
     rfds[*nfds].events = IOPAUSE_READ;
     *nfds += 1;
    }
}

void del_do(iopause_fd *rfds, int *j)
{
 int c;
 for (c = 0;c < CHANNELS;++c)
   if (flagspawnalive[c])
     if (rfds[*j].revents | IOPAUSE_READ) {
       *j += 1;
       del_dochan(c);
     }
}


/* this file is too long -------------------------------------------- PASSES */

struct
 {
  unsigned long id; /* if 0, need a new pass */
  int j; /* defined if id; job number */
  int fd; /* defined if id; reading from {local,remote} */
  off_t mpos; /* defined if id; mark position */
  buffer b;
  char buf[128];
 }
pass[CHANNELS];

void pass_init(void)
{
 int c;
 for (c = 0;c < CHANNELS;++c) pass[c].id = 0;
}

void pass_selprep(tain *wakeup)
{
 int c;
 struct prioq_elt pe;
 if (flagexitasap) return;
 for (c = 0;c < CHANNELS;++c)
   if (pass[c].id)
     if (del_avail(c))
      { *wakeup = tain_zero; return; }
 if (job_avail())
   for (c = 0;c < CHANNELS;++c)
     if (!pass[c].id)
       if (prioq_min(&pqchan[c],&pe))
         if (tain_less(&pe.dt, wakeup))
           *wakeup = pe.dt;
 if (prioq_min(&pqfail,&pe))
   if (tain_less(&pe.dt, wakeup))
     *wakeup = pe.dt;
 if (prioq_min(&pqdone,&pe))
   if (tain_less(&pe.dt, wakeup))
     *wakeup = pe.dt;
}

static uint64_t squareroot(uint64_t x) /* result^2 <= x < (result + 1)^2 */
{
 uint64_t y;
 uint64_t yy;
 uint64_t y21;
 int j;

 y = 0; yy = 0;
 for (j = 15;j >= 0;--j)
  {
   y21 = (y << (j + 1)) + (1 << (j + j));
   if (y21 <= x - yy) { y += (1 << j); yy += y21; }
  }
 return y;
}

void nextretry(tain *result, tain *birth, int c)
{
 uint64_t n;
 tain t;

 if (tain_less(&recent, birth)) n = 0;
 else {
   tain_sub(&t, &recent, birth);
   n = squareroot(tai_sec(tain_secp(&t))); /* no need to add fuzz to recent */
 }
 n += chanskip[c];
 tain_addsec(result, birth, n * n);
}

void pass_dochan(int c)
{
 tain birth, t;
 struct prioq_elt pe;
 static stralloc line = STRALLOC_ZERO;
 int match;

 if (flagexitasap) return;

 if (!pass[c].id)
  {
   if (!job_avail()) return;
   if (!prioq_min(&pqchan[c],&pe)) return;
   if (tain_less(&recent, &pe.dt)) return;
   fnmake_chanaddr(pe.id,c);

   prioq_delmin(&pqchan[c]);
   pass[c].mpos = 0;
   pass[c].fd = open_read(fn.s);
   if (pass[c].fd == -1) goto trouble;
   if (!getinfo(&line,&birth,pe.id)) { close(pass[c].fd); goto trouble; }
   pass[c].id = pe.id;
   buffer_init(&pass[c].b,buffer_read,pass[c].fd,pass[c].buf,sizeof(pass[c].buf));
   pass[c].j = job_open(pe.id,c);
   nextretry(&jo[pass[c].j].retry, &birth,c);
   tain_addsec(&t, &birth, lifetime);
   jo[pass[c].j].flagdying = tain_less(&t, &recent);
   while (!stralloc_copy(&jo[pass[c].j].sender,&line)) nomem();
  }

 if (!del_avail(c)) return;

 if (getln(&pass[c].b,&line,&match,'\0') == -1)
  {
   fnmake_chanaddr(pass[c].id,c);
   log3("warning: trouble reading ",fn.s,"; will try again later\n");
   close(pass[c].fd);
   job_close(pass[c].j);
   pass[c].id = 0;
   return;
  }
 if (!match)
  {
   close(pass[c].fd);
   jo[pass[c].j].flaghiteof = 1;
   job_close(pass[c].j);
   pass[c].id = 0;
   return;
  }
 switch(line.s[0])
  {
   case 'T':
     ++jo[pass[c].j].numtodo;
     del_start(pass[c].j,pass[c].mpos,line.s + 1);
     break;
   case 'D':
     break;
   default:
     fnmake_chanaddr(pass[c].id,c);
     log3("warning: unknown record type in ",fn.s,"!\n");
     close(pass[c].fd);
     job_close(pass[c].j);
     pass[c].id = 0;
     return;
  }

 pass[c].mpos += line.len;
 return;

 trouble:
 log3("warning: trouble opening ",fn.s,"; will try again later\n");
 tain_addsec(&pe.dt, &recent, SLEEP_SYSFAIL);
 while (!prioq_insert(&pqchan[c],&pe)) nomem();
}

void messdone(unsigned long id)
{
 char ch;
 int c;
 struct prioq_elt pe;
 struct stat st;

 for (c = 0;c < CHANNELS;++c)
  {
   fnmake_chanaddr(id,c);
   if (stat(fn.s,&st) == 0) return; /* false alarm; consequence of HOPEFULLY */
   if (errno != ENOENT)
    {
     log3("warning: unable to stat ",fn.s,"; will try again later\n");
     goto fail;
    }
  }

 fnmake_todo(id);
 if (stat(fn.s,&st) == 0) return;
 if (errno != ENOENT)
  {
   log3("warning: unable to stat ",fn.s,"; will try again later\n");
   goto fail;
  }
 
 fnmake_info(id);
 if (stat(fn.s,&st) == -1)
  {
   if (errno == ENOENT) return;
   log3("warning: unable to stat ",fn.s,"; will try again later\n");
   goto fail;
  }
 
 /* -todo +info -local -remote ?bounce */
 if (!injectbounce(id))
   goto fail; /* injectbounce() produced error message */

 strnum3[ulong_fmt(strnum3,id)] = 0;
 log3("end msg ",strnum3,"\n");

 /* -todo +info -local -remote -bounce */
 fnmake_info(id);
 if (unlink(fn.s) == -1)
  {
   log3("warning: unable to unlink ",fn.s,"; will try again later\n");
   goto fail;
  }

 /* -todo -info -local -remote -bounce; we can relax */
 fnmake_foop(id);
 if (buffer_putflush(&btoqc,fn.s,fn.len) == -1) { cleandied(); return; }
 if (buffer_get(&bfromqc,&ch,1) != 1) { cleandied(); return; }
 if (ch != '+')
   log3("warning: qmail-clean unable to clean up ",fn.s,"\n");

 return;

 fail:
 pe.id = id;
 tain_now(&pe.dt);
 tain_addsec(&pe.dt, &pe.dt, SLEEP_SYSFAIL);
 while (!prioq_insert(&pqdone,&pe)) nomem();
}

void pass_do(void)
{
 int c;
 struct prioq_elt pe;

 for (c = 0;c < CHANNELS;++c) pass_dochan(c);
 if (prioq_min(&pqfail,&pe))
   if (!tain_less(&recent, &pe.dt))
    {
     prioq_delmin(&pqfail);
     pqadd(pe.id);
    }
 if (prioq_min(&pqdone,&pe))
   if (!tain_less(&recent, &pe.dt))
    {
     prioq_delmin(&pqdone);
     messdone(pe.id);
    }
}


/* this file is too long ---------------------------------------------- TODO */

tain nexttodorun;
DIR *tododir; /* if 0, have to opendir again */
stralloc todoline = STRALLOC_ZERO;
char todobuf[BUFFER_INSIZE];
char todobufinfo[512];
char todobufchan[CHANNELS][1024];

void todo_init(void)
{
 tododir = 0;
 tain_now(&nexttodorun);
 trigger_set();
}

void todo_selprep(iopause_fd *fd, tain *wakeup)
{
 if (flagexitasap) return;
 trigger_selprep(fd);
 if (tododir) *wakeup = tain_zero;
 if (tain_less(&nexttodorun, wakeup)) *wakeup = nexttodorun;
}

void todo_do(iopause_fd *rfds)
{
 struct stat st;
 buffer b; int fd;
 buffer binfo; int fdinfo;
 buffer bchan[CHANNELS];
 int fdchan[CHANNELS];
 int flagchan[CHANNELS];
 struct prioq_elt pe;
 char ch;
 int match;
 unsigned long id;
 unsigned int len;
 direntry *dent;
 int c;
 unsigned long uid;
 unsigned long pid;

 fd = -1;
 fdinfo = -1;
 for (c = 0;c < CHANNELS;++c) fdchan[c] = -1;

 if (flagexitasap) return;

 if (!tododir)
  {
   if (!trigger_pulled(rfds))
     if (tain_less(&recent, &nexttodorun))
       return;
   trigger_set();
   tododir = opendir("todo");
   if (!tododir)
    {
     pausedir("todo");
     return;
    }
   tain_addsec(&nexttodorun, &recent, SLEEP_TODO);
  }

 dent = readdir(tododir);
 if (!dent)
  {
   closedir(tododir);
   tododir = 0;
   return;
  }
 if (str_equal(dent->d_name,".")) return;
 if (str_equal(dent->d_name,"..")) return;
 len = ulong_scan(dent->d_name,&id);
 if (!len || dent->d_name[len]) return;

 fnmake_todo(id);

 fd = open_read(fn.s);
 if (fd == -1) { log3("warning: unable to open ",fn.s,"\n"); return; }

 fnmake_mess(id);
 /* just for the statistics */
 if (stat(fn.s,&st) == -1)
  { log3("warning: unable to stat ",fn.s,"\n"); goto fail; }

 for (c = 0;c < CHANNELS;++c)
  {
   fnmake_chanaddr(id,c);
   if (unlink(fn.s) == -1) if (errno != ENOENT)
    { log3("warning: unable to unlink ",fn.s,"\n"); goto fail; }
  }

 fnmake_info(id);
 if (unlink(fn.s) == -1) if (errno != ENOENT)
  { log3("warning: unable to unlink ",fn.s,"\n"); goto fail; }

 fdinfo = open_excl(fn.s);
 if (fdinfo == -1)
  { log3("warning: unable to create ",fn.s,"\n"); goto fail; }

 strnum3[ulong_fmt(strnum3,id)] = 0;
 log3("new msg ",strnum3,"\n");

 for (c = 0;c < CHANNELS;++c) flagchan[c] = 0;

 buffer_init(&b,buffer_read,fd,todobuf,sizeof(todobuf));
 buffer_init(&binfo,buffer_write,fdinfo,todobufinfo,sizeof(todobufinfo));

 uid = 0;
 pid = 0;

 for (;;)
  {
   if (getln(&b,&todoline,&match,'\0') == -1)
    {
     /* perhaps we're out of memory, perhaps an I/O error */
     fnmake_todo(id);
     log3("warning: trouble reading ",fn.s,"\n"); goto fail;
    }
   if (!match) break;

   switch(todoline.s[0])
    {
     case 'u':
       ulong_scan(todoline.s + 1,&uid);
       break;
     case 'p':
       ulong_scan(todoline.s + 1,&pid);
       break;
     case 'F':
       if (buffer_putflush(&binfo,todoline.s,todoline.len) == -1)
	{
	 fnmake_info(id);
         log3("warning: trouble writing to ",fn.s,"\n"); goto fail;
	}
       qslog2("info msg ",strnum3);
       strnum2[ulong_fmt(strnum2,(unsigned long) st.st_size)] = 0;
       qslog2(": bytes ",strnum2);
       log1(" from <"); logsafe(todoline.s + 1);
       strnum2[ulong_fmt(strnum2,pid)] = 0;
       qslog2("> qp ",strnum2);
       strnum2[ulong_fmt(strnum2,uid)] = 0;
       qslog2(" uid ",strnum2);
       log1("\n");
       break;
     case 'T':
       switch(rewrite(todoline.s + 1))
	{
	 case 0: nomem(); goto fail;
	 case 2: c = 1; break;
	 default: c = 0; break;
        }
       if (fdchan[c] == -1)
	{
	 fnmake_chanaddr(id,c);
	 fdchan[c] = open_excl(fn.s);
	 if (fdchan[c] == -1)
          { log3("warning: unable to create ",fn.s,"\n"); goto fail; }
	 buffer_init(&bchan[c]
	   ,buffer_write,fdchan[c],todobufchan[c],sizeof(todobufchan[c]));
	 flagchan[c] = 1;
	}
       if (buffer_put(&bchan[c],rwline.s,rwline.len) == -1)
        {
	 fnmake_chanaddr(id,c);
         log3("warning: trouble writing to ",fn.s,"\n"); goto fail;
        }
       break;
     default:
       fnmake_todo(id);
       log3("warning: unknown record type in ",fn.s,"\n"); goto fail;
    }
  }

 close(fd); fd = -1;

 fnmake_info(id);
 if (buffer_flush(&binfo) == -1)
  { log3("warning: trouble writing to ",fn.s,"\n"); goto fail; }
 if (fsync(fdinfo) == -1)
  { log3("warning: trouble fsyncing ",fn.s,"\n"); goto fail; }
 close(fdinfo); fdinfo = -1;

 for (c = 0;c < CHANNELS;++c)
   if (fdchan[c] != -1)
    {
     fnmake_chanaddr(id,c);
     if (buffer_flush(&bchan[c]) == -1)
      { log3("warning: trouble writing to ",fn.s,"\n"); goto fail; }
     if (fsync(fdchan[c]) == -1)
      { log3("warning: trouble fsyncing ",fn.s,"\n"); goto fail; }
     close(fdchan[c]); fdchan[c] = -1;
    }

 fnmake_todo(id);
 if (buffer_putflush(&btoqc,fn.s,fn.len) == -1) { cleandied(); return; }
 if (buffer_get(&bfromqc,&ch,1) != 1) { cleandied(); return; }
 if (ch != '+')
  {
   log3("warning: qmail-clean unable to clean up ",fn.s,"\n");
   return;
  }

 pe.id = id; tain_now(&pe.dt);
 for (c = 0;c < CHANNELS;++c)
   if (flagchan[c])
     while (!prioq_insert(&pqchan[c],&pe)) nomem();

 for (c = 0;c < CHANNELS;++c) if (flagchan[c]) break;
 if (c == CHANNELS)
   while (!prioq_insert(&pqdone,&pe)) nomem();

 return;

 fail:
 if (fd != -1) close(fd);
 if (fdinfo != -1) close(fdinfo);
 for (c = 0;c < CHANNELS;++c)
   if (fdchan[c] != -1) close(fdchan[c]);
}


/* this file is too long ---------------------------------------------- MAIN */

int getcontrols(void) { if (control_init() == -1) return 0;
 if (control_readint(&lifetime,"control/queuelifetime") == -1) return 0;
 if (control_readint(&concurrency[0],"control/concurrencylocal") == -1) return 0;
 if (control_readint(&concurrency[1],"control/concurrencyremote") == -1) return 0;
 if (control_rldef(&envnoathost,"control/envnoathost",1,"envnoathost") != 1) return 0;
 if (control_rldef(&bouncefrom,"control/bouncefrom",0,"MAILER-DAEMON") != 1) return 0;
 if (control_rldef(&bouncehost,"control/bouncehost",1,"bouncehost") != 1) return 0;
 if (control_rldef(&doublebouncehost,"control/doublebouncehost",1,"doublebouncehost") != 1) return 0;
 if (control_rldef(&doublebounceto,"control/doublebounceto",0,"postmaster") != 1) return 0;
 if (!stralloc_cats(&doublebounceto,"@")) return 0;
 if (!stralloc_cat(&doublebounceto,&doublebouncehost)) return 0;
 if (!stralloc_0(&doublebounceto)) return 0;
 if (control_readfile(&locals,"control/locals",1) != 1) return 0;
 if (!constmap_init(&maplocals,locals.s,locals.len,0)) return 0;
 switch(control_readfile(&percenthack,"control/percenthack",0))
  {
   case -1: return 0;
   case 0: if (!constmap_init(&mappercenthack,"",0,0)) return 0; break;
   case 1: if (!constmap_init(&mappercenthack,percenthack.s,percenthack.len,0)) return 0; break;
  }
 switch(control_readfile(&vdoms,"control/virtualdomains",0))
  {
   case -1: return 0;
   case 0: if (!constmap_init(&mapvdoms,"",0,1)) return 0; break;
   case 1: if (!constmap_init(&mapvdoms,vdoms.s,vdoms.len,1)) return 0; break;
  }
 return 1; }

stralloc newlocals = STRALLOC_ZERO;
stralloc newvdoms = STRALLOC_ZERO;

void regetcontrols(void)
{
 int r;

 if (control_readfile(&newlocals,"control/locals",1) != 1)
  { log1("alert: unable to reread control/locals\n"); return; }
 r = control_readfile(&newvdoms,"control/virtualdomains",0);
 if (r == -1)
  { log1("alert: unable to reread control/virtualdomains\n"); return; }

 constmap_free(&maplocals);
 constmap_free(&mapvdoms);

 while (!stralloc_copy(&locals,&newlocals)) nomem();
 while (!constmap_init(&maplocals,locals.s,locals.len,0)) nomem();

 if (r)
  {
   while (!stralloc_copy(&vdoms,&newvdoms)) nomem();
   while (!constmap_init(&mapvdoms,vdoms.s,vdoms.len,1)) nomem();
  }
 else
   while (!constmap_init(&mapvdoms,"",0,1)) nomem();
}

void reread(void)
{
 if (chdir(auto_qmail) == -1)
  {
   log1("alert: unable to reread controls: unable to switch to home directory\n");
   return;
  }
 regetcontrols();
 while (chdir("queue") == -1)
  {
   log1("alert: unable to switch back to queue directory; HELP! sleeping...\n");
   sleep(10);
  }
}

int main(void)
{
  int fd;
  tain wakeup;
  iopause_fd fds[CHANNELS + CHANNELS + 1];
  int nfds;
  int c;

  if (chdir(auto_qmail) == -1)
  { log1("alert: cannot start: unable to switch to home directory\n"); _exit(111); }
  if (!getcontrols())
  { log1("alert: cannot start: unable to read controls\n"); _exit(111); }
  if (chdir("queue") == -1)
  { log1("alert: cannot start: unable to switch to queue directory\n"); _exit(111); }
  sig_ignore(SIGPIPE);
  sig_catch(SIGTERM, sigterm);
  sig_catch(SIGALRM, sigalrm);
  sig_catch(SIGHUP, sighup);
  sig_restore(SIGCHLD);
  umask(077);

  fd = open_write("lock/sendmutex");
  if (fd == -1)
  { log1("alert: cannot start: unable to open mutex\n"); _exit(111); }
  if (lock_exnb(fd) == -1)
  { log1("alert: cannot start: qmail-send is already running\n"); _exit(111); }

  numjobs = 0;
  for (c = 0;c < CHANNELS;++c) {
    char ch;
    int u;
    int r;
    r = fd_read(chanfdin[c],&ch,1);
    if (r < 1)
    { log1("alert: cannot start: hath the daemon spawn no fire?\n"); _exit(111); }
    u = (unsigned int) (unsigned char) ch;
    if (concurrency[c] > u) concurrency[c] = u;
    numjobs += concurrency[c];
  }

  fnmake_init();

  comm_init();

  pqstart();
  job_init();
  del_init();
  pass_init();
  todo_init();
  cleanup_init();

  while (!flagexitasap || !del_canexit())
  {
    tain_now(&recent);

    if (flagrunasap) { flagrunasap = 0; pqrun(); }
    if (flagreadasap) { flagreadasap = 0; reread(); }

    tain_addsec(&wakeup, &recent, SLEEP_FOREVER);

    nfds = 0;

    comm_selprep(&nfds,fds);
    del_selprep(&nfds,fds + nfds);
    pass_selprep(&wakeup);
    todo_selprep(fds + nfds, &wakeup);
    nfds++;
    cleanup_selprep(&wakeup);

    if (!tain_less(&recent, &wakeup)) wakeup = recent;
    else tain_addsec(&wakeup, &wakeup, SLEEP_FUZZ);

    if (iopause_stamp(fds, nfds, &wakeup, &recent) == -1)
      if (errno == EINTR)
        ;
      else
        log1("warning: trouble in select\n");
    else {
      int j = 0;
      comm_do(fds, &j);
      del_do(fds + j, &j);
      todo_do(fds + j);
      pass_do();
      cleanup_do();
    }
  }
  pqfinish();
  log1("status: exiting\n");
  _exit(0);
}
