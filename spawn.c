#include <errno.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/uio.h>

#include <skalibs/sig.h>
#include <skalibs/buffer.h>
#include <skalibs/djbunix.h>
#include <skalibs/stralloc.h>
#include <skalibs/alloc.h>
#include <skalibs/iopause.h>
#include <skalibs/bytestr.h>
#include <skalibs/allreadwrite.h>

#include "spawn.h"

#include "auto_qmail.h"
#include "auto_uids.h"
#include "auto_spawn.h"

uid_t auto_uidq;

struct delivery
 {
  int used;
  int fdin; /* pipe input */
  int pid; /* zero if child is dead */
  int wstat; /* if !pid: status of child */
  int fdout; /* pipe output, -1 if !pid; delays eof until after death */
  stralloc output;
 }
;

struct delivery *d;

void sigchld()
{
 int wstat;
 int pid;
 int i;
 while ((pid = wait_nohang(&wstat)) > 0)
   for (i = 0;i < auto_spawn;++i) if (d[i].used)
     if (d[i].pid == pid)
      {
       close(d[i].fdout); d[i].fdout = -1;
       d[i].wstat = wstat; d[i].pid = 0;
      }
}

int flagwriting = 1;

ssize_t okwrite(int fd, struct iovec const *vbuf, unsigned int n)
{
 int w;
 if (!flagwriting) return n;
 w = writev(fd,vbuf,n);
 if (w != -1) return w;
 if (errno == EINTR) return -1;
 flagwriting = 0; close(fd);
 return n;
}

int flagreading = 1;
char outbuf[1024];
buffer bout;

int stage = 0; /* reading 0:delnum 1:messid 2:sender 3:recip */
int flagabort = 0; /* if 1, everything except delnum is garbage */
int delnum;
stralloc messid = STRALLOC_ZERO;
stralloc sender = STRALLOC_ZERO;
stralloc recip = STRALLOC_ZERO;

void err(char *s)
{
 char ch; ch = delnum; buffer_put(&bout,&ch,1);
 buffer_puts(&bout,s); buffer_putflush(&bout,"",1);
}

void docmd()
{
 int f;
 int i;
 int j;
 int fdmess;
 int pi[2];
 struct stat st;

 if (flagabort) { err("Zqmail-spawn out of memory. (#4.3.0)\n"); return; }
 if (delnum < 0) { err("ZInternal error: delnum negative. (#4.3.5)\n"); return; }
 if (delnum >= auto_spawn) { err("ZInternal error: delnum too big. (#4.3.5)\n"); return; }
 if (d[delnum].used) { err("ZInternal error: delnum in use. (#4.3.5)\n"); return; }
 for (i = 0;i < messid.len;++i)
   if (messid.s[i])
     if (!i || (messid.s[i] != '/'))
       if ((unsigned char) (messid.s[i] - '0') > 9)
        { err("DInternal error: messid has nonnumerics. (#5.3.5)\n"); return; }
 if (messid.len > 100) { err("DInternal error: messid too long. (#5.3.5)\n"); return; }
 if (!messid.s[0]) { err("DInternal error: messid too short. (#5.3.5)\n"); return; }

 if (!stralloc_copys(&d[delnum].output,""))
  { err("Zqmail-spawn out of memory. (#4.3.0)\n"); return; }

 j = byte_rchr(recip.s,recip.len,'@');
 if (j >= recip.len) { err("DSorry, address must include host name. (#5.1.3)\n"); return; }

 fdmess = open_read(messid.s);
 if (fdmess == -1) { err("Zqmail-spawn unable to open message. (#4.3.0)\n"); return; }

 if (fstat(fdmess,&st) == -1)
  { close(fdmess); err("Zqmail-spawn unable to fstat message. (#4.3.0)\n"); return; }
 if ((st.st_mode & S_IFMT) != S_IFREG)
  { close(fdmess); err("ZSorry, message has wrong type. (#4.3.5)\n"); return; }
 if (st.st_uid != auto_uidq) /* aaack! qmailq has to be trusted! */
  /* your security is already toast at this point. damage control... */
  { close(fdmess); err("ZSorry, message has wrong owner. (#4.3.5)\n"); return; }

 if (pipe(pi) == -1)
  { close(fdmess); err("Zqmail-spawn unable to create pipe. (#4.3.0)\n"); return; }

 coe(pi[0]);

 f = spawn(fdmess,pi[1],sender.s,recip.s,j);
 close(fdmess);
 if (f == -1)
  { close(pi[0]); close(pi[1]); err("Zqmail-spawn unable to fork. (#4.3.0)\n"); return; }

 d[delnum].fdin = pi[0];
 d[delnum].fdout = pi[1]; coe(pi[1]);
 d[delnum].pid = f;
 d[delnum].used = 1;
}

char cmdbuf[1024];

void getcmd()
{
 int i;
 int r;
 char ch;

 r = read(0,cmdbuf,sizeof(cmdbuf));
 if (r == 0)
  { flagreading = 0; return; }
 if (r == -1)
  {
   if (errno != EINTR)
     flagreading = 0;
   return;
  }
 
 for (i = 0;i < r;++i)
  {
   ch = cmdbuf[i];
   switch(stage)
    {
     case 0:
       delnum = (unsigned int) (unsigned char) ch;
       messid.len = 0; stage = 1; break;
     case 1:
       if (!stralloc_append(&messid,ch)) flagabort = 1;
       if (ch) break;
       sender.len = 0; stage = 2; break;
     case 2:
       if (!stralloc_append(&sender,ch)) flagabort = 1;
       if (ch) break;
       recip.len = 0; stage = 3; break;
     case 3:
       if (!stralloc_append(&recip,ch)) flagabort = 1;
       if (ch) break;
       docmd();
       flagabort = 0; stage = 0; break;
    }
  }
}

char inbuf[128];

int main(int argc, char **argv)
{
 char ch;
 int i;
 int r;
 iopause_fd *rfds;
 unsigned int nfds;

 if (chdir(auto_qmail) == -1) _exit(111);
 if (chdir("queue/mess") == -1) _exit(111);
 if (!stralloc_copys(&messid,"")) _exit(111);
 if (!stralloc_copys(&sender,"")) _exit(111);
 if (!stralloc_copys(&recip,"")) _exit(111);

 d = (struct delivery *) alloc((auto_spawn + 10) * sizeof(struct delivery));
 if (!d) _exit(111);

 rfds = (iopause_fd *) alloc((auto_spawn + 1) * sizeof(iopause_fd));
 if (!rfds) _exit(111);

 buffer_init(&bout,okwrite,1,outbuf,sizeof(outbuf));

 sig_ignore(SIGPIPE);
 sig_catch(SIGCHLD, sigchld);

 initialize(argc,argv);

 ch = auto_spawn; buffer_putflush(&bout,&ch,1);

 for (i = 0;i < auto_spawn;++i) { d[i].used = 0; d[i].output.s = 0; }

 for (;;) { /* XXX: should use self-pipe instead of messing with SIGCHLD */
   if (!flagreading) {
     for (i = 0;i < auto_spawn;++i) if (d[i].used) break;
     if (i >= auto_spawn) _exit(0);
   }
   sig_unblock(SIGCHLD);

   nfds = 0;
   if (flagreading) {
     rfds[0].fd = 0;
     rfds[0].events = IOPAUSE_READ;
     nfds = 1;
   }
   for (i = 0;i < auto_spawn;++i)
     if (d[i].used) {
       rfds[nfds].fd = d[i].fdin;
       rfds[nfds].events = IOPAUSE_READ;
       nfds++;
     }

   r = iopause(rfds, nfds, NULL, NULL);
   sig_block(SIGCHLD);

   if (r != -1) {
     int j = 0;
     if (flagreading)
       if (rfds[0].revents | IOPAUSE_READ) {
         getcmd();
         j = 1;
       }
     for (i = 0;i < auto_spawn;++i)
       if (d[i].used) {
         if (rfds[j].revents | IOPAUSE_READ) {
           r = read(d[i].fdin,inbuf,128);
           if (r == -1)
             continue; /* read error on a readable pipe? be serious */
           if (r == 0) {
             ch = i; buffer_put(&bout,&ch,1);
             report(&bout,d[i].wstat,d[i].output.s,d[i].output.len);
             buffer_put(&bout,"",1);
             buffer_flush(&bout);
             close(d[i].fdin); d[i].used = 0;
             continue;
           }
           while (!stralloc_readyplus(&d[i].output,r)) sleep(10); /*XXX*/
           byte_copy(d[i].output.s + d[i].output.len,r,inbuf);
           d[i].output.len += r;
           if (truncreport > 100)
             if (d[i].output.len > truncreport) {
               char *truncmess = "\nError report too long, sorry.\n";
               d[i].output.len = truncreport - str_len(truncmess) - 3;
               stralloc_cats(&d[i].output,truncmess);
             }
         }
         j++;
       }
   }
 }
}
