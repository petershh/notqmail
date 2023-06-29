#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <skalibs/stralloc.h>
#include <skalibs/bytestr.h>
#include <skalibs/buffer.h>
#include <skalibs/sig.h>
#include <skalibs/types.h>
#include <skalibs/direntry.h>
#include <skalibs/tai.h>

#include "getln.h"
#include "auto_qmail.h"
#include "fmtqfn.h"

#define OSSIFIED 129600 /* see qmail-send.c */

stralloc line = STRALLOC_ZERO;

void cleanuppid(void)
{
 DIR *dir;
 direntry *d;
 struct stat st;
 tain time;
 tain atime;

 tain_now(&time);
 dir = opendir("pid");
 if (!dir) return;
 while ((d = readdir(dir)))
  {
   if (str_equal(d->d_name,".")) continue;
   if (str_equal(d->d_name,"..")) continue;
   if (!stralloc_copys(&line,"pid/")) continue;
   if (!stralloc_cats(&line,d->d_name)) continue;
   if (!stralloc_0(&line)) continue;
   if (stat(line.s,&st) == -1) continue;
   tain_from_timespec_sysclock(&atime, &st.st_atim);
   tain_addsec(&atime, &atime, OSSIFIED);
   if (tain_less(&time, &atime)) continue;
   unlink(line.s);
  }
 closedir(dir);
}

char fnbuf[FMTQFN];

void respond(s) char *s; { if (buffer_putflush(buffer_1small,s,1) == -1) _exit(100); }

int main(void)
{
 int i;
 int match;
 int cleanuploop;
 unsigned long id;

 if (chdir(auto_qmail) == -1) return 111;
 if (chdir("queue") == -1) return 111;

 sig_ignore(SIGPIPE);

 if (!stralloc_ready(&line,200)) return 111;

 cleanuploop = 0;

 for (;;)
  {
   if (cleanuploop) --cleanuploop; else { cleanuppid(); cleanuploop = 30; }
   if (getln(buffer_0small,&line,&match,'\0') == -1) break;
   if (!match) break;
   if (line.len < 7) { respond("x"); continue; }
   if (line.len > 100) { respond("x"); continue; }
   if (line.s[line.len - 1]) { respond("x"); continue; } /* impossible */
   for (i = 5;i < line.len - 1;++i)
     if ((unsigned char) (line.s[i] - '0') > 9)
      { respond("x"); continue; }
   if (!ulong_scan(line.s + 5,&id)) { respond("x"); continue; }
   if (byte_equal(line.s,5,"foop/"))
    {
#define U(prefix,flag) fmtqfn(fnbuf,prefix,id,flag); \
if (unlink(fnbuf) == -1) if (errno != ENOENT) { respond("!"); continue; }
     U("intd/",0)
     U("mess/",1)
     respond("+");
    }
   else if (byte_equal(line.s,4,"todo/"))
    {
     U("intd/",0)
     U("todo/",0)
     respond("+");
    }
   else
     respond("x");
  }
 return 0;
}
