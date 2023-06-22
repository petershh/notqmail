#include <errno.h>

#include <unistd.h>

#include <skalibs/stralloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/buffer.h>
#include <skalibs/types.h>
#include <skalibs/cdb.h>
#include <skalibs/bytestr.h>
#include <skalibs/error.h>

/*
#include "fd.h"
#include "substdio.h"
#include "stralloc.h"
#include "scan.h"
#include "exit.h"
#include "fork.h"
#include "error.h"
#include "byte.h"
#include "cdb.h"
#include "case.h"
#include "open.h"
*/

#include "spawn.h"
#include "prot.h"
#include "wait.h"
#include "slurpclose.h"
#include "uidgid.h"
#include "auto_qmail.h"
#include "auto_uids.h"
#include "auto_users.h"
#include "qlx.h"

char *aliasempty;

uid_t auto_uidp;

gid_t auto_gidn;

void initialize(int argc, char **argv)
{
  aliasempty = argv[1];
  if (!aliasempty) _exit(100);

  auto_uidp = inituid(auto_userp);
  auto_uidq = inituid(auto_userq);

  auto_gidn = initgid(auto_groupn);
}

int truncreport = 3000;

void report(buffer *b, int wstat, char *s, int len)
{
 int i;
 if (wait_crashed(wstat))
  { buffer_puts(b,"Zqmail-local crashed.\n"); return; }
 switch(wait_exitcode(wstat))
  {
   case QLX_CDB:
     buffer_puts(b,"ZTrouble reading users/cdb in qmail-lspawn.\n"); return;
   case QLX_NOMEM:
     buffer_puts(b,"ZOut of memory in qmail-lspawn.\n"); return;
   case QLX_SYS:
     buffer_puts(b,"ZTemporary failure in qmail-lspawn.\n"); return;
   case QLX_NOALIAS:
     buffer_puts(b,"ZUnable to find alias user!\n"); return;
   case QLX_ROOT:
     buffer_puts(b,"ZNot allowed to perform deliveries as root.\n"); return;
   case QLX_USAGE:
     buffer_puts(b,"ZInternal qmail-lspawn bug.\n"); return;
   case QLX_NFS:
     buffer_puts(b,"ZNFS failure in qmail-local.\n"); return;
   case QLX_EXECHARD:
     buffer_puts(b,"DUnable to run qmail-local.\n"); return;
   case QLX_EXECSOFT:
     buffer_puts(b,"ZUnable to run qmail-local.\n"); return;
   case QLX_EXECPW:
     buffer_puts(b,"ZUnable to run qmail-getpw.\n"); return;
   case 111: case 71: case 74: case 75:
     buffer_put(b,"Z",1); break;
   case 0:
     buffer_put(b,"K",1); break;
   case 100:
   default:
     buffer_put(b,"D",1); break;
  }

 for (i = 0;i < len;++i) if (!s[i]) break;
 buffer_put(b,s,i);
}

stralloc lower = STRALLOC_ZERO;
stralloc nughde = STRALLOC_ZERO;
stralloc wildchars = STRALLOC_ZERO;

void nughde_get(char *local)
{
 char *(args[3]);
 int pi[2];
 int gpwpid;
 int gpwstat;
 int r;
 int fd;
 int flagwild;
 cdb db = CDB_ZERO;

 if (!stralloc_copys(&lower,"!")) _exit(QLX_NOMEM);
 if (!stralloc_cats(&lower,local)) _exit(QLX_NOMEM);
 if (!stralloc_0(&lower)) _exit(QLX_NOMEM);
 case_lowerb(lower.s,lower.len);

 if (!stralloc_copys(&nughde,"")) _exit(QLX_NOMEM);

 fd = open_read("users/cdb");
 if (fd == -1)
   if (errno != ENOENT)
     _exit(QLX_CDB);

 if (fd != -1)
  {
   unsigned int i;
   cdb_data data;
   
   r = cdb_init_fromfd(&db, fd);
   
   if (r == 0) _exit(errno == ENOMEM ? QLX_NOMEM : QLX_CDB);

   r = cdb_find(&db, &data, "", 0);
   if (r != 1) _exit(QLX_CDB);
   if (stralloc_catb(&wildchars,data.s,data.len) == 0) _exit(QLX_NOMEM);

   i = lower.len;
   flagwild = 0;

   do
    {
     /* i > 0 */
     if (!flagwild || (i == 1) || (byte_chr(wildchars.s,wildchars.len,lower.s[i - 1]) < wildchars.len))
      {
       r = cdb_find(&db, &data, lower.s, i);
       if (r == 0) _exit(QLX_CDB);
       if (r == 1)
        {
         if (stralloc_catb(&nughde, data.s, data.len) == 0) _exit(QLX_NOMEM);
         if (flagwild)
	   if (!stralloc_cats(&nughde,local + i - 1)) _exit(QLX_NOMEM);
         if (!stralloc_0(&nughde)) _exit(QLX_NOMEM);
         close(fd);
         return;
        }
      }
     --i;
     flagwild = 1;
    }
   while (i);

   cdb_free(&db);
   close(fd);
  }

 if (pipe(pi) == -1) _exit(QLX_SYS);
 args[0] = "bin/qmail-getpw";
 args[1] = local;
 args[2] = 0;
 switch(gpwpid = fork())
  {
   case -1:
     _exit(QLX_SYS);
   case 0:
     if (prot_gid(auto_gidn) == -1) _exit(QLX_USAGE);
     if (prot_uid(auto_uidp) == -1) _exit(QLX_USAGE);
     close(pi[0]);
     if (fd_move(1,pi[1]) == -1) _exit(QLX_SYS);
     execv(*args,args);
     _exit(QLX_EXECPW);
  }
 close(pi[1]);

 if (slurpclose(pi[0],&nughde,128) == -1) _exit(QLX_SYS);

 if (wait_pid(gpwpid,&gpwstat) != -1)
  {
   if (wait_crashed(gpwstat)) _exit(QLX_SYS);
   if (wait_exitcode(gpwstat) != 0) _exit(wait_exitcode(gpwstat));
  }
}

int spawn(int fdmess, int fdout, char *s, char *r, int at)
{
 int f;

 if (!(f = fork()))
  {
   char *(args[11]);
   unsigned long u;
   int n;
   uid_t uid;
   gid_t gid;
   char *x;
   unsigned int xlen;
   
   r[at] = 0;
   if (!r[0]) _exit(0); /* <> */

   if (chdir(auto_qmail) == -1) _exit(QLX_USAGE);

   nughde_get(r);

   x = nughde.s;
   xlen = nughde.len;

   args[0] = "bin/qmail-local";
   args[1] = "--";
   args[2] = x;
   n = byte_chr(x,xlen,0); if (n++ == xlen) _exit(QLX_USAGE); x += n; xlen -= n;

   ulong_scan(x,&u);
   uid = u;
   n = byte_chr(x,xlen,0); if (n++ == xlen) _exit(QLX_USAGE); x += n; xlen -= n;

   ulong_scan(x,&u);
   gid = u;
   n = byte_chr(x,xlen,0); if (n++ == xlen) _exit(QLX_USAGE); x += n; xlen -= n;

   args[3] = x;
   n = byte_chr(x,xlen,0); if (n++ == xlen) _exit(QLX_USAGE); x += n; xlen -= n;

   args[4] = r;
   args[5] = x;
   n = byte_chr(x,xlen,0); if (n++ == xlen) _exit(QLX_USAGE); x += n; xlen -= n;

   args[6] = x;
   n = byte_chr(x,xlen,0); if (n++ == xlen) _exit(QLX_USAGE); x += n; xlen -= n;

   args[7] = r + at + 1;
   args[8] = s;
   args[9] = aliasempty;
   args[10] = 0;

   if (fd_move(0,fdmess) == -1) _exit(QLX_SYS);
   if (fd_move(1,fdout) == -1) _exit(QLX_SYS);
   if (fd_copy(2,1) == -1) _exit(QLX_SYS);
   if (prot_gid(gid) == -1) _exit(QLX_USAGE);
   if (prot_uid(uid) == -1) _exit(QLX_USAGE);
   if (!getuid()) _exit(QLX_ROOT);

   execv(*args,args);
   if (error_temp(errno)) _exit(QLX_EXECSOFT);
   _exit(QLX_EXECHARD);
  }
 return f;
}
