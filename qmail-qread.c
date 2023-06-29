#include <errno.h>
#include <string.h>
#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <skalibs/stralloc.h>
#include <skalibs/buffer.h>
#include <skalibs/bytestr.h>
#include <skalibs/djbunix.h>
#include <skalibs/types.h>
#include <skalibs/tai.h>
#include <skalibs/djbtime.h>

#include "fmt.h"
#include "getln.h"
#include "fmtqfn.h"
#include "readsubdir.h"
#include "auto_qmail.h"
#include "date822fmt.h"
#include "noreturn.h"

#define STATS_FMT DATE822FMT + 7 + ULONG_FMT + 2 + ULONG_FMT + 3 + 2 + 9

readsubdir rs;

void _noreturn_ die(int n) { buffer_flush(buffer_1); _exit(n); }

void warn(char *s1, char *s2)
{
 char *x;
 x = strerror(errno);
 buffer_puts(buffer_1,s1);
 buffer_puts(buffer_1,s2);
 buffer_puts(buffer_1,": ");
 buffer_puts(buffer_1,x);
 buffer_puts(buffer_1,"\n");
}

void _noreturn_ die_nomem() { buffer_puts(buffer_1,"fatal: out of memory\n"); die(111); }
void _noreturn_ die_chdir() { warn("fatal: unable to chdir",""); die(111); }
void _noreturn_ die_opendir(char *fn) { warn("fatal: unable to opendir ",fn); die(111); }

void err(unsigned long id)
{
 char foo[ULONG_FMT];
 foo[ulong_fmt(foo,id)] = 0;
 warn("warning: trouble with #",foo);
}

char fnmess[FMTQFN];
char fninfo[FMTQFN];
char fnlocal[FMTQFN];
char fnremote[FMTQFN];
char fnbounce[FMTQFN];

char inbuf[1024];
stralloc sender = STRALLOC_ZERO;

unsigned long id;
tain qtime;
int flagbounce;
unsigned long size;

unsigned int fmtstats(char *s)
{
 struct tm dt;
 unsigned int len;
 unsigned int i;

 len = 0;
 localtm_from_tai(&dt, tain_secp(&qtime), 0);
 i = date822fmt(s,&dt) - 7/*XXX*/; len += i; if (s) s += i;
 i = fmt_str(s," GMT  #"); len += i; if (s) s += i;
 i = ulong_fmt(s,id); len += i; if (s) s += i;
 i = fmt_str(s,"  "); len += i; if (s) s += i;
 i = ulong_fmt(s,size); len += i; if (s) s += i;
 i = fmt_str(s,"  <"); len += i; if (s) s += i;
 i = fmt_str(s,sender.s + 1); len += i; if (s) s += i;
 i = fmt_str(s,"> "); len += i; if (s) s += i;
 if (flagbounce)
  {
   i = fmt_str(s," bouncing"); len += i; if (s) s += i;
  }

 return len;
}

stralloc stats = STRALLOC_ZERO;

void out(char *s, unsigned int n)
{
 while (n > 0)
  {
   buffer_put(buffer_1,((*s >= 32) && (*s <= 126)) ? s : "_",1);
   --n;
   ++s;
  }
}
void outs(char *s) { out(s,str_len(s)); }
void outok(char *s) { buffer_puts(buffer_1,s); }

void putstats(void)
{
 if (!stralloc_ready(&stats, STATS_FMT + str_len(sender.s) - 1)) die_nomem();
 stats.len = fmtstats(stats.s);
 out(stats.s,stats.len);
 outok("\n");
}

stralloc line = STRALLOC_ZERO;

int main(void)
{
 int channel;
 int match;
 struct stat st;
 int fd;
 buffer b;
 int x;

 if (chdir(auto_qmail) == -1) die_chdir();
 if (chdir("queue") == -1) die_chdir();
 readsubdir_init(&rs,"info",die_opendir);

 while ((x = readsubdir_next(&rs,&id)))
   if (x > 0)
    {
     fmtqfn(fnmess,"mess/",id,1);
     fmtqfn(fninfo,"info/",id,1);
     fmtqfn(fnlocal,"local/",id,1);
     fmtqfn(fnremote,"remote/",id,1);
     fmtqfn(fnbounce,"bounce/",id,0);

     if (stat(fnmess,&st) == -1) { err(id); continue; }
     size = st.st_size;
     flagbounce = !stat(fnbounce,&st);

     fd = open_read(fninfo);
     if (fd == -1) { err(id); continue; }
     buffer_init(&b,buffer_read,fd,inbuf,sizeof(inbuf));
     if (getln(&b,&sender,&match,0) == -1) die_nomem();
     if (fstat(fd,&st) == -1) { close(fd); err(id); continue; }
     close(fd);
     tain_from_timespec_sysclock(&qtime, &st.st_mtim);

     putstats();

     for (channel = 0;channel < 2;++channel) {
       fd = open_read(channel ? fnremote : fnlocal);
       if (fd == -1) {
         if (errno != ENOENT)
           err(id);
       }
       else {
         for (;;) {
           if (getln(&b,&line,&match,0) == -1) die_nomem();
           if (!match) break;
           switch(line.s[0]) {
             case 'D':
               outok("  done");
             case 'T':
               outok(channel ? "\tremote\t" : "\tlocal\t");
               outs(line.s + 1);
               outok("\n");
               break;
           }
         }
         close(fd);
       }
     }
    }

 die(0);
}
