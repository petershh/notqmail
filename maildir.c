#include <time.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <skalibs/env.h>
#include <skalibs/stralloc.h>
#include <skalibs/tai.h>
#include <skalibs/djbtime.h>
#include <skalibs/direntry.h>
#include <skalibs/genalloc.h>

/*
#include "env.h"
#include "stralloc.h"
#include "str.h"
#include "now.h"
#include "datetime.h"
*/

#include "maildir.h"

#include "prioq.h"

struct strerr maildir_chdir_err;
struct strerr maildir_scan_err;

int maildir_chdir(void)
{
 char const *maildir;
 maildir = env_get("MAILDIR");
 if (!maildir)
   STRERR(-1,maildir_chdir_err,"MAILDIR not set")
 if (chdir(maildir) == -1)
   STRERR_SYS3(-1,maildir_chdir_err,"unable to chdir to ",maildir,": ")
 return 0;
}

void maildir_clean(stralloc *tmpname)
{
 DIR *dir;
 direntry *d;
 tai time;
 struct stat st;

 tai_now(&time);

 dir = opendir("tmp");
 if (!dir) return;

 while ((d = readdir(dir)))
  {
   if (d->d_name[0] == '.') continue;
   if (!stralloc_copys(tmpname,"tmp/")) break;
   if (!stralloc_cats(tmpname,d->d_name)) break;
   if (!stralloc_0(tmpname)) break;
   if (stat(tmpname->s,&st) == 0) {
     tai delta, atime;
     tai_uint(&delta, 129600);

     tai_from_time(&atime, st.st_atime);
     tai_add(&atime, &atime, &delta);
     if (tai_less(&atime, &time))
       unlink(tmpname->s);
   }
  }
 closedir(dir);
}

static int append(genalloc *pq, stralloc *filenames, char *subdir, tai const *time)
{
 DIR *dir;
 direntry *d;
 struct prioq_elt pe;
 unsigned int pos;
 struct stat st;

 dir = opendir(subdir);
 if (!dir)
   STRERR_SYS3(-1,maildir_scan_err,"unable to scan $MAILDIR/",subdir,": ")

 while ((d = readdir(dir)))
  {
   if (d->d_name[0] == '.') continue;
   pos = filenames->len;
   if (!stralloc_cats(filenames,subdir)) break;
   if (!stralloc_cats(filenames,"/")) break;
   if (!stralloc_cats(filenames,d->d_name)) break;
   if (!stralloc_0(filenames)) break;
   if (stat(filenames->s + pos,&st) == 0) {
     tai mtime;
     tai_from_time(&mtime, st.st_mtime);
     if (tai_less(&mtime, time)) { /* don't want to mix up the order */
       pe.dt = mtime;
       pe.id = pos;
       if (!prioq_insert(pq,&pe)) break;
      }
   }
  }

 closedir(dir);
 if (d) STRERR_SYS3(-1,maildir_scan_err,"unable to read $MAILDIR/",subdir,": ")
 return 0;
}

int maildir_scan(genalloc *pq, stralloc *filenames, int flagnew, int flagcur)
{
 struct prioq_elt pe;
 tai time;

 if (!stralloc_copys(filenames,"")) return 0;
 while (prioq_min(pq,&pe)) prioq_delmin(pq);

 tai_now(&time);

 if (flagnew) if (append(pq,filenames,"new",&time) == -1) return -1;
 if (flagcur) if (append(pq,filenames,"cur",&time) == -1) return -1;
 return 0;
}
