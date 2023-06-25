#include <stdio.h>
#include <string.h>

#include <sys/stat.h>
#include <unistd.h>

/*
#include "readwrite.h"
#include "env.h"
#include "subfd.h"
#include "error.h"
#include "open.h"
#include "stralloc.h"
#include "lock.h"
#include "str.h"

extern int rename(const char *, const char *);
*/


#include <skalibs/stralloc.h>
#include <skalibs/buffer.h>
#include <skalibs/strerr.h>
#include <skalibs/env.h>
#include <skalibs/djbunix.h>
#include <skalibs/genalloc.h>

#include "getln.h"
#include "prioq.h"
#include "strerr.h"
#include "myctime.h"
#include "gfrom.h"
#include "maildir.h"
#include "substdio.h"

char const *mbox;
char const *mboxtmp;

stralloc filenames = STRALLOC_ZERO;
genalloc pq = GENALLOC_ZERO;
genalloc pq2 = GENALLOC_ZERO;

stralloc line = STRALLOC_ZERO;

stralloc ufline = STRALLOC_ZERO;

char inbuf[BUFFER_INSIZE];
char outbuf[BUFFER_OUTSIZE];

#define FATAL "maildir2mbox: fatal: "
#define WARNING "maildir2mbox: warning: "

void die_nomem() { strerr_dief1x(111, "out of memory"); }

int main(void)
{
 buffer ssin;
 buffer ssout;
 struct prioq_elt pe;
 int fdoldmbox;
 int fdnewmbox;
 int fd;
 int match;
 int fdlock;

 PROG = "maildir2mbox";

 umask(077);

 mbox = env_get("MAIL");
 if (!mbox) strerr_dief1x(111, "MAIL not set");
 mboxtmp = env_get("MAILTMP");
 if (!mboxtmp) strerr_dief1x(111, "MAILTMP not set");

 if (maildir_chdir() == -1)
   strerr_die1(111, FATAL, &maildir_chdir_err);
 maildir_clean(&filenames);
 if (maildir_scan(&pq,&filenames,1,1) == -1)
   strerr_die1(111, FATAL, &maildir_scan_err);

 if (!prioq_min(&pq, &pe)) return 0; /* nothing new */

 fdlock = open_append(mbox);
 if (fdlock == -1)
   strerr_diefu2sys(111, "lock ", mbox);
 if (fd_lock(fdlock, 1, 0) == -1)
   strerr_diefu2sys(111, "lock ", mbox);

 fdoldmbox = open_read(mbox);
 if (fdoldmbox == -1)
   strerr_diefu2sys(111, "read ", mbox);

 fdnewmbox = open_trunc(mboxtmp);
 if (fdnewmbox == -1)
   strerr_diefu2sys(111, "create ", mboxtmp);

 buffer_init(&ssin, buffer_read, fdoldmbox, inbuf, sizeof(inbuf));
 buffer_init(&ssout, buffer_write, fdnewmbox, outbuf, sizeof(outbuf));

 switch(buffer_copy(&ssout, &ssin))
  {
   case -2: strerr_diefu2sys(111, "read ", mbox);
   case -3: strerr_diefu2sys(111, "write to ", mboxtmp);
  }

 while (prioq_min(&pq, &pe))
  {
   prioq_delmin(&pq);
   if (!prioq_insert(&pq2, &pe)) die_nomem();

   fd = open_read(filenames.s + pe.id);
   if (fd == -1)
     strerr_diefu2sys(111, "read $MAILDIR/",filenames.s + pe.id);
   buffer_init(&ssin, buffer_read, fd, inbuf, sizeof(inbuf));

   if (getln(&ssin,&line,&match,'\n') != 0)
     strerr_diefu2sys(111, "read $MAILDIR/", filenames.s + pe.id);
   
   if (!stralloc_copys(&ufline,"From XXX ")) die_nomem();
   if (match)
     if (strncmp(line.s, "Return-Path: <", line.len))
      {
       if (line.s[14] == '>')
	{
         if (!stralloc_copys(&ufline,"From MAILER-DAEMON ")) die_nomem();
	}
       else
	{
	 int i;
         if (!stralloc_ready(&ufline,line.len)) die_nomem();
         if (!stralloc_copys(&ufline,"From ")) die_nomem();
	 for (i = 14;i < line.len - 2;++i)
	   if ((line.s[i] == ' ') || (line.s[i] == '\t'))
	     ufline.s[ufline.len++] = '-';
	   else
	     ufline.s[ufline.len++] = line.s[i];
         if (!stralloc_cats(&ufline," ")) die_nomem();
	}
      }
   if (!stralloc_cats(&ufline,myctime(pe.dt))) die_nomem();
   if (buffer_put(&ssout,ufline.s,ufline.len) == -1)
     strerr_diefu2sys(111, "write to ", mboxtmp);

   while (match && line.len)
    {
     if (gfrom(line.s,line.len))
       if (buffer_puts(&ssout,">") == -1)
         strerr_diefu2sys(111, "write to ", mboxtmp);
     if (buffer_put(&ssout,line.s,line.len) == -1)
       strerr_diefu2sys(111, "write to ", mboxtmp);
     if (!match)
      {
       if (buffer_puts(&ssout,"\n") == -1)
         strerr_diefu2sys(111, "write to ", mboxtmp);
       break;
      }
     if (getln(&ssin, &line, &match, '\n') != 0)
       strerr_diefu2sys(111, "read $MAILDIR/", filenames.s + pe.id);
    }
   if (buffer_puts(&ssout, "\n"))
     strerr_diefu2sys(111, "write to ", mboxtmp);

   close(fd);
  }

 if (buffer_flush(&ssout) == -1)
   strerr_diefu2sys(111, "write to ", mboxtmp);
 if (fsync(fdnewmbox) == -1)
   strerr_diefu2sys(111, "write to ", mboxtmp);
 if (close(fdnewmbox) == -1) /* NFS dorks */
   strerr_diefu2sys(111, "write to ", mboxtmp);
 if (rename(mboxtmp,mbox) == -1)
   strerr_die6(111,FATAL,"unable to move ",mboxtmp," to ",mbox,": ",&strerr_sys);
 
 while (prioq_min(&pq2,&pe))
  {
   prioq_delmin(&pq2);
   if (unlink(filenames.s + pe.id) == -1)
     strerr_warn4(WARNING,"$MAILDIR/",filenames.s + pe.id," will be delivered twice; unable to unlink: ",&strerr_sys);
  }

 return 0;
}
