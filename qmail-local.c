#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>

#include <skalibs/sig.h>
#include <skalibs/env.h>
#include <skalibs/strerr.h>
#include <skalibs/buffer.h>
#include <skalibs/bytestr.h>
#include <skalibs/stralloc.h>
#include <skalibs/sgetopt.h>
#include <skalibs/types.h>
#include <skalibs/djbunix.h>
#include <skalibs/error.h>
#include <skalibs/exec.h>
#include <skalibs/tai.h>

#include "buffer_copy.h"
#include "getln.h"
#include "fmt.h"
#include "quote.h"
#include "qmail.h"
#include "myctime.h"
#include "gfrom.h"
#include "auto_patrn.h"
#include "wait.h"
#include "slurpclose.h"
#include "noreturn.h"
#include "lock.h"

#define USAGE "qmail-local [ -nN ] user homedir local dash ext domain sender aliasempty"


void _noreturn_ usage() { strerr_dieusage (100, USAGE); }

void _noreturn_ temp_nomem() { strerr_dief1x(111,"Out of memory. (#4.3.0)"); }
void _noreturn_ temp_rewind() { strerr_dief1x(111,"Unable to rewind message. (#4.3.0)"); }
void _noreturn_ temp_childcrashed() { strerr_dief1x(111,"Aack, child crashed. (#4.3.0)"); }
void _noreturn_ temp_fork() { strerr_dief3x(111,"Unable to fork: ",strerror(errno),". (#4.3.0)"); }
void _noreturn_ temp_read() { strerr_dief3x(111,"Unable to read message: ",strerror(errno),". (#4.3.0)"); }
void _noreturn_ temp_slowlock()
{ strerr_dief1x(111,"File has been locked for 30 seconds straight. (#4.3.0)"); }
void _noreturn_ temp_qmail(char *fn)
{ strerr_dief5x(111,"Unable to open ",fn,": ",strerror(errno),". (#4.3.0)"); }

int flagdoit;
int flag99;

char *user;
char *homedir;
char *local;
char *dash;
char *ext;
char *host;
char *sender;
char *aliasempty;

stralloc safeext = STRALLOC_ZERO;
stralloc ufline = STRALLOC_ZERO;
stralloc rpline = STRALLOC_ZERO;
stralloc envrecip = STRALLOC_ZERO;
stralloc dtline = STRALLOC_ZERO;
stralloc qme = STRALLOC_ZERO;
stralloc ueo = STRALLOC_ZERO;
stralloc cmds = STRALLOC_ZERO;
stralloc messline = STRALLOC_ZERO;
stralloc foo = STRALLOC_ZERO;

char buf[1024];
char outbuf[1024];

/* child process */

char fntmptph[80 + TAIN_FMT * 2];
char fnnewtph[80 + TAIN_FMT * 2];
void tryunlinktmp() { unlink(fntmptph); }
void sigalrm() { tryunlinktmp(); _exit(3); }

void maildir_child(char *dir)
{
 unsigned long pid;
 tain time;
 char myhost[64];
 char *s;
 int loop;
 int fd;
 buffer b;
 buffer bout;

 sig_catch(SIGALRM, sigalrm);
 if (chdir(dir) == -1) { if (error_temp(errno)) _exit(1); _exit(2); }
 pid = getpid();
 myhost[0] = 0;
 gethostname(myhost,sizeof(myhost));
 for (loop = 0;;++loop)
  {
   tain_now(&time);
   s = fntmptph;
   s += fmt_str(s,"tmp/");
   s += tain_fmt(s,&time); *s++ = '.';
   s += ulong_fmt(s,pid); *s++ = '.';
   s += fmt_strn(s,myhost,sizeof(myhost)); *s++ = 0;
   alarm(86400);
   fd = open_excl(fntmptph);
   if (fd >= 0)
     break;
   if (errno == EEXIST) {
     /* really should never get to this point */
     if (loop == 2) _exit(1);
     sleep(2);
   } else {
     _exit(1);
   }
  }
 str_copy(fnnewtph,fntmptph);
 byte_copy(fnnewtph,3,"new");

 buffer_init(&b, buffer_read, 0, buf, sizeof(buf));
 buffer_init(&bout, buffer_write, fd, outbuf, sizeof(outbuf));
 if (buffer_put(&bout,rpline.s,rpline.len) == -1) goto fail;
 if (buffer_put(&bout,dtline.s,dtline.len) == -1) goto fail;

 switch(buffer_copy(&bout,&b))
  {
   case -2: tryunlinktmp(); _exit(4);
   case -3: goto fail;
  }

 if (buffer_flush(&bout) == -1) goto fail;
 if (fsync(fd) == -1) goto fail;
 if (close(fd) == -1) goto fail; /* NFS dorks */

 if (link(fntmptph,fnnewtph) == -1) goto fail;
   /* if it was error_exist, almost certainly successful; i hate NFS */
 tryunlinktmp(); _exit(0);

 fail: tryunlinktmp(); _exit(1);
}

/* end child process */

void maildir(char *fn)
{
 int child;
 int wstat;

 if (lseek(0, 0, SEEK_SET) == -1) temp_rewind();

 switch(child = fork())
  {
   case -1:
     temp_fork();
   case 0:
     maildir_child(fn);
     _exit(111);
  }

 wait_pid(child,&wstat);
 if (wait_crashed(wstat))
   temp_childcrashed();
 switch(wait_exitcode(wstat))
  {
   case 0: break;
   case 2: strerr_dief1x(111,"unable to chdir to maildir. (#4.2.1)");
   case 3: strerr_dief1x(111,"timeout on maildir delivery. (#4.3.0)");
   case 4: strerr_dief1x(111,"unable to read message. (#4.3.0)");
   default: strerr_dief1x(111,"temporary error on maildir delivery. (#4.3.0)");
  }
}

void mailfile(char *fn)
{
 int fd;
 buffer b;
 buffer bout;
 int match;
 off_t pos;
 int flaglocked;

 if (lseek(0, 0, SEEK_SET) == -1) temp_rewind();

 fd = open_append(fn);
 if (fd == -1)
   strerr_die(111,"Unable to open ",fn,": ",strerror(errno),". (#4.2.1)");

 sig_catch(SIGALRM, temp_slowlock);
 alarm(30);
 flaglocked = (lock_ex(fd) != -1);
 alarm(0);
 sig_restore(SIGALRM);

 lseek(fd, 0, SEEK_END);
 pos = lseek(fd, 0, SEEK_CUR);

 buffer_init(&b, buffer_read, 0, buf, sizeof(buf));
 buffer_init(&bout, buffer_write,fd, outbuf, sizeof(outbuf));
 if (buffer_put(&bout,ufline.s,ufline.len)) goto writeerrs;
 if (buffer_put(&bout,rpline.s,rpline.len)) goto writeerrs;
 if (buffer_put(&bout,dtline.s,dtline.len)) goto writeerrs;
 for (;;)
  {
   if (getln(&b,&messline,&match,'\n') != 0)
    {
     strerr_warnwu3x("read message: ",strerror(errno),". (#4.3.0)");
     if (flaglocked) ftruncate(fd,pos); close(fd);
     _exit(111);
    }
   if (!match && !messline.len) break;
   if (gfrom(messline.s,messline.len))
     if (buffer_put(&bout,">",1)) goto writeerrs;
   if (buffer_put(&bout,messline.s,messline.len)) goto writeerrs;
   if (!match)
    {
     if (buffer_puts(&bout,"\n")) goto writeerrs;
     break;
    }
  }
 if (buffer_puts(&bout,"\n")) goto writeerrs;
 if (buffer_flush(&bout)) goto writeerrs;
 if (fsync(fd) == -1) goto writeerrs;
 close(fd);
 return;

 writeerrs:
 strerr_warnwu5x("write ",fn,": ",strerror(errno),". (#4.3.0)");
 if (flaglocked) ftruncate(fd,pos);
 close(fd);
 _exit(111);
}

void mailprogram(char *prog, stralloc *env_modifs)
{
 int child;
 char *(args[4]);
 int wstat;

 if (lseek(0, 0, SEEK_SET) == -1) temp_rewind();

 switch(child = fork())
  {
   case -1:
     temp_fork();
   case 0:
     args[0] = "/bin/sh"; args[1] = "-c"; args[2] = prog; args[3] = 0;
     sig_restore(SIGPIPE);
     mexec_m((char const *const *)args, env_modifs->s, env_modifs->len);
     strerr_diefu3x(111,"run /bin/sh: ",strerror(errno),". (#4.3.0)");
  }

 wait_pid(child, &wstat);
 if (wait_crashed(wstat))
   temp_childcrashed();
 switch(wait_exitcode(wstat))
  {
   case 100:
   case 64: case 65: case 70: case 76: case 77: case 78: case 112: _exit(100);
   case 0: break;
   case 99: flag99 = 1; break;
   default: _exit(111);
  }
}

unsigned long mailforward_qp = 0;

void mailforward(char **recips)
{
 struct qmail qqt;
 char *qqx;
 buffer b;
 int match;

 if (lseek(0, 0, SEEK_SET) == -1) temp_rewind();
 buffer_init(&b,buffer_read,0,buf,sizeof(buf));

 if (qmail_open(&qqt) == -1) temp_fork();
 mailforward_qp = qmail_qp(&qqt);
 qmail_put(&qqt,dtline.s,dtline.len);
 do
  {
   if (getln(&b,&messline,&match,'\n') != 0) { qmail_fail(&qqt); break; }
   qmail_put(&qqt,messline.s,messline.len);
  }
 while (match);
 qmail_from(&qqt,ueo.s);
 while (*recips) qmail_to(&qqt,*recips++);
 qqx = qmail_close(&qqt);
 if (!*qqx) return;
 strerr_diefu3x(*qqx == 'D' ? 100 : 111,"forward message: ",qqx + 1,".");
}

void bouncexf(void)
{
 int match;
 buffer b;

 if (lseek(0, 0, SEEK_SET) == -1) temp_rewind();
 buffer_init(&b,buffer_read,0,buf,sizeof(buf));
 for (;;)
  {
   if (getln(&b,&messline,&match,'\n') != 0) temp_read();
   if (!match) break;
   if (messline.len <= 1)
     break;
   if (messline.len == dtline.len)
     if (!str_diffn(messline.s,dtline.s,dtline.len))
       strerr_dief1x(100,"this message is looping: it already has my Delivered-To line. (#5.4.6)");
  }
}

void checkhome(void)
{
 struct stat st;

 if (stat(".",&st) == -1)
   strerr_diefu3x(111,"stat home directory: ",strerror(errno),". (#4.3.0)");
 if (st.st_mode & auto_patrn)
   strerr_dief1x(111,"uh-oh: home directory is writable. (#4.7.0)");
 if (st.st_mode & 01000) {
   if (flagdoit)
     strerr_dief1x(111,"home directory is sticky: user is editing his .qmail file. (#4.2.1)");
   else
     strerr_warnw1x("home directory is sticky.");
 }
}

int qmeox(char *dashowner)
{
 struct stat st;

 if (!stralloc_copys(&qme,".qmail")) temp_nomem();
 if (!stralloc_cats(&qme,dash)) temp_nomem();
 if (!stralloc_cat(&qme,&safeext)) temp_nomem();
 if (!stralloc_cats(&qme,dashowner)) temp_nomem();
 if (!stralloc_0(&qme)) temp_nomem();
 if (stat(qme.s,&st) == -1)
  {
   if (error_temp(errno)) temp_qmail(qme.s);
   return -1;
  }
 return 0;
}

int qmeexists(int *fd, int *cutable)
{
  struct stat st;

  if (!stralloc_0(&qme)) temp_nomem();

  *fd = open_read(qme.s);
  if (*fd == -1) {
    if (error_temp(errno)) temp_qmail(qme.s);
    if (errno == EPERM) temp_qmail(qme.s);
    if (errno == EACCES) temp_qmail(qme.s);
    return 0;
  }

  if (fstat(*fd,&st) == -1) temp_qmail(qme.s);
  if ((st.st_mode & S_IFMT) == S_IFREG) {
    if (st.st_mode & auto_patrn)
      strerr_dief1x(111,"uh-oh: .qmail file is writable. (#4.7.0)");
    *cutable = !!(st.st_mode & 0100);
    return 1;
  }
  close(*fd);
  return 0;
}

/* "" "": "" */
/* "-/" "": "-/" "-/default" */
/* "-/" "a": "-/a" "-/default" */
/* "-/" "a-": "-/a-" "-/a-default" "-/default" */
/* "-/" "a-b": "-/a-b" "-/a-default" "-/default" */
/* "-/" "a-b-": "-/a-b-" "-/a-b-default" "-/a-default" "-/default" */
/* "-/" "a-b-c": "-/a-b-c" "-/a-b-default" "-/a-default" "-/default" */

void qmesearch(int *fd, int *cutable, stralloc *env_modifs)
{
  int i;

  if (!stralloc_copys(&qme,".qmail")) temp_nomem();
  if (!stralloc_cats(&qme,dash)) temp_nomem();
  if (!stralloc_cat(&qme,&safeext)) temp_nomem();
  if (qmeexists(fd,cutable)) {
    if (safeext.len >= 7) {
      i = safeext.len - 7;
      if (byte_equal("default",7,safeext.s + i))
	if (i <= str_len(ext)) /* paranoia */
	  if (!env_addmodif(env_modifs,"DEFAULT",ext + i)) temp_nomem();
    }
    return;
  }

  for (i = safeext.len;i >= 0;--i)
    if (!i || (safeext.s[i - 1] == '-')) {
      if (!stralloc_copys(&qme,".qmail")) temp_nomem();
      if (!stralloc_cats(&qme,dash)) temp_nomem();
      if (!stralloc_catb(&qme,safeext.s,i)) temp_nomem();
      if (!stralloc_cats(&qme,"default")) temp_nomem();
      if (qmeexists(fd,cutable)) {
	if (i <= str_len(ext)) /* paranoia */
	  if (!env_addmodif(env_modifs,"DEFAULT",ext + i)) temp_nomem();
        return;
      }
    }

  *fd = -1;
}

unsigned long count_file = 0;
unsigned long count_forward = 0;
unsigned long count_program = 0;
char count_buf[ULONG_FMT];

void count_print()
{
 buffer_puts(buffer_1small,"did ");
 buffer_put(buffer_1small,count_buf,ulong_fmt(count_buf,count_file));
 buffer_puts(buffer_1small,"+");
 buffer_put(buffer_1small, count_buf,ulong_fmt(count_buf,count_forward));
 buffer_puts(buffer_1small,"+");
 buffer_put(buffer_1small,count_buf,ulong_fmt(count_buf,count_program));
 buffer_puts(buffer_1small,"\n");
 if (mailforward_qp)
  {
   buffer_puts(buffer_1small,"qp ");
   buffer_put(buffer_1small,count_buf,ulong_fmt(count_buf,mailforward_qp));
   buffer_puts(buffer_1small,"\n");
  }
 buffer_flush(buffer_1small);
}

void sayit(char *type, char *cmd, unsigned int len)
{
 buffer_puts(buffer_1small,type);
 buffer_put(buffer_1small,cmd,len);
 buffer_putsflush(buffer_1small,"\n");
}

int main(int argc, char **argv)
{
 int opt;
 unsigned int i;
 unsigned int j;
 int fd;
 unsigned int numforward;
 char **recips;
 tain starttime;
 int flagforwardonly;
 char *x;
 subgetopt l = SUBGETOPT_ZERO;
 stralloc env_modifs = STRALLOC_ZERO;

 umask(077);
 sig_ignore(SIGPIPE);
/*
 if (!env_init()) temp_nomem();
*/
 flagdoit = 1;
 while ((opt = subgetopt_r(argc,(char const *const *)argv,"nN",&l)) != -1)
   switch(opt)
    {
     case 'n': flagdoit = 0; break;
     case 'N': flagdoit = 1; break;
     default:
       usage();
    }
 argc -= l.ind;
 argv += l.ind;

 if (!(user = *argv++)) usage();
 if (!(homedir = *argv++)) usage();
 if (!(local = *argv++)) usage();
 if (!(dash = *argv++)) usage();
 if (!(ext = *argv++)) usage();
 if (!(host = *argv++)) usage();
 if (!(sender = *argv++)) usage();
 if (!(aliasempty = *argv++)) usage();
 if (*argv) usage();

 if (homedir[0] != '/') usage();
 if (chdir(homedir) == -1)
   strerr_diefu5x(111,"switch to ",homedir,": ",strerror(errno),". (#4.3.0)");
 checkhome();

 if (!env_addmodif(&env_modifs,"HOST",host)) temp_nomem();
 if (!env_addmodif(&env_modifs,"HOME",homedir)) temp_nomem();
 if (!env_addmodif(&env_modifs,"USER",user)) temp_nomem();
 if (!env_addmodif(&env_modifs,"LOCAL",local)) temp_nomem();

 if (!stralloc_copys(&envrecip,local)) temp_nomem();
 if (!stralloc_cats(&envrecip,"@")) temp_nomem();
 if (!stralloc_cats(&envrecip,host)) temp_nomem();

 if (!stralloc_copy(&foo,&envrecip)) temp_nomem();
 if (!stralloc_0(&foo)) temp_nomem();
 if (!env_addmodif(&env_modifs,"RECIPIENT",foo.s)) temp_nomem();

 if (!stralloc_copys(&dtline,"Delivered-To: ")) temp_nomem();
 if (!stralloc_cat(&dtline,&envrecip)) temp_nomem();
 for (i = 0;i < dtline.len;++i) if (dtline.s[i] == '\n') dtline.s[i] = '_';
 if (!stralloc_cats(&dtline,"\n")) temp_nomem();

 if (!stralloc_copy(&foo,&dtline)) temp_nomem();
 if (!stralloc_0(&foo)) temp_nomem();
 if (!env_addmodif(&env_modifs,"DTLINE",foo.s)) temp_nomem();

 if (flagdoit)
   bouncexf();

 if (!env_addmodif(&env_modifs,"SENDER",sender)) temp_nomem();

 if (!quote2(&foo,sender)) temp_nomem();
 if (!stralloc_copys(&rpline,"Return-Path: <")) temp_nomem();
 if (!stralloc_cat(&rpline,&foo)) temp_nomem();
 for (i = 0;i < rpline.len;++i) if (rpline.s[i] == '\n') rpline.s[i] = '_';
 if (!stralloc_cats(&rpline,">\n")) temp_nomem();

 if (!stralloc_copy(&foo,&rpline)) temp_nomem();
 if (!stralloc_0(&foo)) temp_nomem();
 if (!env_addmodif(&env_modifs,"RPLINE",foo.s)) temp_nomem();

 if (!stralloc_copys(&ufline,"From ")) temp_nomem();
 if (*sender)
  {
   unsigned int len;
   char ch;

   len = str_len(sender);
   if (!stralloc_readyplus(&ufline,len)) temp_nomem();
   for (i = 0;i < len;++i)
    {
     ch = sender[i];
     if ((ch == ' ') || (ch == '\t') || (ch == '\n')) ch = '-';
     ufline.s[ufline.len + i] = ch;
    }
   ufline.len += len;
  }
 else
   if (!stralloc_cats(&ufline,"MAILER-DAEMON")) temp_nomem();
 if (!stralloc_cats(&ufline," ")) temp_nomem();
 tain_now(&starttime);
 if (!stralloc_cats(&ufline,myctime(tain_secp(&starttime)))) temp_nomem();

 if (!stralloc_copy(&foo,&ufline)) temp_nomem();
 if (!stralloc_0(&foo)) temp_nomem();
 if (!env_addmodif(&env_modifs,"UFLINE",foo.s)) temp_nomem();

 x = ext;
 if (!env_addmodif(&env_modifs,"EXT",x)) temp_nomem();
 x += str_chr(x,'-'); if (*x) ++x;
 if (!env_addmodif(&env_modifs,"EXT2",x)) temp_nomem();
 x += str_chr(x,'-'); if (*x) ++x;
 if (!env_addmodif(&env_modifs,"EXT3",x)) temp_nomem();
 x += str_chr(x,'-'); if (*x) ++x;
 if (!env_addmodif(&env_modifs,"EXT4",x)) temp_nomem();

 if (!stralloc_copys(&safeext,ext)) temp_nomem();
 case_lowerb(safeext.s,safeext.len);
 for (i = 0;i < safeext.len;++i)
   if (safeext.s[i] == '.')
     safeext.s[i] = ':';

 i = str_len(host);
 i = byte_rchr(host,i,'.');
 if (!stralloc_copyb(&foo,host,i)) temp_nomem();
 if (!stralloc_0(&foo)) temp_nomem();
 if (!env_addmodif(&env_modifs,"HOST2",foo.s)) temp_nomem();
 i = byte_rchr(host,i,'.');
 if (!stralloc_copyb(&foo,host,i)) temp_nomem();
 if (!stralloc_0(&foo)) temp_nomem();
 if (!env_addmodif(&env_modifs,"HOST3",foo.s)) temp_nomem();
 i = byte_rchr(host,i,'.');
 if (!stralloc_copyb(&foo,host,i)) temp_nomem();
 if (!stralloc_0(&foo)) temp_nomem();
 if (!env_addmodif(&env_modifs,"HOST4",foo.s)) temp_nomem();

 flagforwardonly = 0;
 qmesearch(&fd,&flagforwardonly,&env_modifs);
 if (fd == -1)
   if (*dash)
     strerr_dief1x(100,"sorry, no mailbox here by that name. (#5.1.1)");

 if (!stralloc_copys(&ueo,sender)) temp_nomem();
 if (str_diff(sender,""))
   if (str_diff(sender,"#@[]"))
     if (qmeox("-owner") == 0)
      {
       if (qmeox("-owner-default") == 0)
	{
         if (!stralloc_copys(&ueo,local)) temp_nomem();
         if (!stralloc_cats(&ueo,"-owner-@")) temp_nomem();
         if (!stralloc_cats(&ueo,host)) temp_nomem();
         if (!stralloc_cats(&ueo,"-@[]")) temp_nomem();
	}
       else
	{
         if (!stralloc_copys(&ueo,local)) temp_nomem();
         if (!stralloc_cats(&ueo,"-owner@")) temp_nomem();
         if (!stralloc_cats(&ueo,host)) temp_nomem();
	}
      }
 if (!stralloc_0(&ueo)) temp_nomem();
 if (!env_addmodif(&env_modifs,"NEWSENDER",ueo.s)) temp_nomem();

 if (!stralloc_ready(&cmds,0)) temp_nomem();
 cmds.len = 0;
 if (fd != -1)
   if (slurpclose(fd,&cmds,256) == -1) temp_nomem();

 if (!cmds.len)
  {
   if (!stralloc_copys(&cmds,aliasempty)) temp_nomem();
   flagforwardonly = 0;
  }
 if (!cmds.len || (cmds.s[cmds.len - 1] != '\n'))
   if (!stralloc_cats(&cmds,"\n")) temp_nomem();

 numforward = 0;
 i = 0;
 for (j = 0;j < cmds.len;++j)
   if (cmds.s[j] == '\n')
    {
     switch(cmds.s[i]) { case '#': case '.': case '/': case '|': break;
       default: ++numforward; }
     i = j + 1;
    }

 recips = calloc(numforward + 1, sizeof(char *));
 if (!recips) temp_nomem();
 numforward = 0;

 flag99 = 0;

 i = 0;
 for (j = 0;j < cmds.len;++j)
   if (cmds.s[j] == '\n')
    {
     unsigned int k = j;
     cmds.s[j] = 0;
     while ((k > i) && ((cmds.s[k - 1] == ' ') || (cmds.s[k - 1] == '\t')))
       cmds.s[--k] = 0;
     switch(cmds.s[i])
      {
       case 0: /* k == i */
	 if (i) break;
         strerr_dief1x(111,"uh-oh: first line of .qmail file is blank. (#4.2.1)");
       case '#':
         break;
       case '.':
       case '/':
	 ++count_file;
	 if (flagforwardonly) strerr_dief1x(111,"uh-oh: .qmail has file delivery but has x bit set. (#4.7.0)");
	 if (cmds.s[k - 1] == '/')
           if (flagdoit) maildir(cmds.s + i);
           else sayit("maildir ",cmds.s + i,k - i);
	 else
           if (flagdoit) mailfile(cmds.s + i);
           else sayit("mbox ",cmds.s + i,k - i);
         break;
       case '|':
	 ++count_program;
	 if (flagforwardonly) strerr_dief1x(111,"uh-oh: .qmail has prog delivery but has x bit set. (#4.7.0)");
         if (flagdoit) mailprogram(cmds.s + i + 1, &env_modifs);
         else sayit("program ",cmds.s + i + 1,k - i - 1);
         break;
       case '+':
	 if (str_equal(cmds.s + i + 1,"list"))
	   flagforwardonly = 1;
	 break;
       case '&':
         ++i;
       default:
	 ++count_forward;
         if (flagdoit) recips[numforward++] = cmds.s + i;
         else sayit("forward ",cmds.s + i,k - i);
         break;
      }
     i = j + 1;
     if (flag99) break;
    }

 if (numforward) if (flagdoit)
  {
   recips[numforward] = 0;
   mailforward(recips);
  }

 count_print();
 _exit(0);
}
