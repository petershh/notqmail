#include <unistd.h>
#include <signal.h>
#include <sys/uio.h>

#include <skalibs/stralloc.h>
#include <skalibs/buffer.h>
#include <skalibs/bytestr.h>
#include <skalibs/types.h>
#include <skalibs/env.h>
#include <skalibs/sig.h>

/*
#include "stralloc.h"
#include "substdio.h"
#include "str.h"
#include "env.h"
#include "scan.h"
#include "sig.h"
#include "readwrite.h"
#include "exit.h"
*/

#include "fmt.h"
#include "qmail.h"
#include "now.h"
#include "noreturn.h"
#include "rcpthosts.h"
#include "auto_qmail.h"
#include "control.h"
#include "received.h"

void _noreturn_ badproto() { _exit(100); }
void _noreturn_ resources() { _exit(111); }

ssize_t safewrite(int fd, const struct iovec *vbuf, unsigned int len)
{
  ssize_t r;
  r = writev(fd,vbuf,len);
  if (r == 0 || r == -1) _exit(0);
  return r;
}

char boutbuf[256];
buffer bout = BUFFER_INIT(safewrite,1,boutbuf,sizeof(boutbuf));

ssize_t saferead(int fd, const struct iovec *vbuf, unsigned int len)
{
  ssize_t r;
  buffer_flush(&bout);
  r = readv(fd,vbuf,len);
  if (r == 0 || r == -1) _exit(0);
  return r;
}

char binbuf[512];
buffer bin = BUFFER_INIT(saferead,0,binbuf,sizeof(binbuf));

unsigned long getlen(void)
{
  unsigned long len = 0;
  char ch;
  for (;;) {
    buffer_get(&bin,&ch,1);
    if (ch == ':') return len;
    if (len > 200000000) resources();
    len = 10 * len + (ch - '0');
  }
}

void getcomma(void)
{
  char ch;
  buffer_get(&bin,&ch,1);
  if (ch != ',') badproto();
}

unsigned int databytes = 0;
unsigned int bytestooverflow = 0;
struct qmail qq;

char buf[1000];
char buf2[100];

char const *remotehost;
char const *remoteinfo;
char const *remoteip;
char const *local;

stralloc failure = STRALLOC_ZERO;

char const *relayclient;
int relayclientlen;

int
main(void)
{
  char ch;
  int i;
  unsigned long biglen;
  unsigned long len;
  int flagdos;
  int flagsenderok;
  int flagbother;
  unsigned long qp;
  char *result;
  char const *x;
  unsigned long u;
 
  sig_ignore(SIGPIPE);
  sig_catch(SIGALRM, resources);
  alarm(3600);
 
  if (chdir(auto_qmail) == -1) resources();
 
  if (control_init() == -1) resources();
  if (rcpthosts_init() == -1) resources();
  relayclient = env_get("RELAYCLIENT");
  relayclientlen = relayclient ? str_len(relayclient) : 0;
 
  if (control_readint(&databytes,"control/databytes") == -1) resources();
  x = env_get("DATABYTES");
  if (x) { ulong_scan(x,&u); databytes = u; }
  if (!(databytes + 1)) --databytes;
 
  remotehost = env_get("TCPREMOTEHOST");
  if (!remotehost) remotehost = "unknown";
  remoteinfo = env_get("TCPREMOTEINFO");
  remoteip = env_get("TCPREMOTEIP");
  if (!remoteip) remoteip = "unknown";
  local = env_get("TCPLOCALHOST");
  if (!local) local = env_get("TCPLOCALIP");
  if (!local) local = "unknown";
 
  for (;;) {
    if (!stralloc_copys(&failure,"")) resources();
    flagsenderok = 1;
 
    len = getlen();
    if (len == 0) badproto();
 
    if (databytes) bytestooverflow = databytes + 1;
    if (qmail_open(&qq) == -1) resources();
    qp = qmail_qp(&qq);
 
    buffer_get(&bin,&ch,1);
    --len;
    if (ch == 10) flagdos = 0;
    else if (ch == 13) flagdos = 1;
    else badproto();
 
    received(&qq,"QMTP",local,remoteip,remotehost,remoteinfo,NULL);
 
    /* XXX: check for loops? only if len is big? */
 
    if (flagdos)
      while (len > 0) {
        buffer_get(&bin,&ch,1);
        --len;
        while ((ch == 13) && len) {
          buffer_get(&bin,&ch,1);
          --len;
          if (ch == 10) break;
          if (bytestooverflow) if (!--bytestooverflow) qmail_fail(&qq);
          qmail_put(&qq,"\015",1);
        }
        if (bytestooverflow) if (!--bytestooverflow) qmail_fail(&qq);
        qmail_put(&qq,&ch,1);
      }
    else {
      if (databytes)
        if (len > databytes) {
          bytestooverflow = 0;
          qmail_fail(&qq);
        }
      while (len > 0) { /* XXX: could speed this up, obviously */
        buffer_get(&bin,&ch,1);
        --len;
        qmail_put(&qq,&ch,1);
      }
    }
    getcomma();
 
    len = getlen();
 
    if (len >= 1000) {
      buf[0] = 0;
      flagsenderok = 0;
      for (i = 0;i < len;++i)
        buffer_get(&bin,&ch,1);
    }
    else {
      for (i = 0;i < len;++i) {
        buffer_get(&bin,buf + i,1);
        if (!buf[i]) flagsenderok = 0;
      }
      buf[len] = 0;
    }
    getcomma();
 
    flagbother = 0;
    qmail_from(&qq,buf);
    if (!flagsenderok) qmail_fail(&qq);
 
    biglen = getlen();
    while (biglen > 0) {
      if (!stralloc_append(&failure,'\0')) resources();
 
      len = 0;
      for (;;) {
        if (!biglen) badproto();
        buffer_get(&bin,&ch,1);
        --biglen;
        if (ch == ':') break;
        if (len > 200000000) resources();
        len = 10 * len + (ch - '0');
      }
      if (len >= biglen) badproto();
      if (len + relayclientlen >= 1000) {
        failure.s[failure.len - 1] = 'L';
        for (i = 0;i < len;++i)
          buffer_get(&bin,&ch,1);
      }
      else {
        for (i = 0;i < len;++i) {
          buffer_get(&bin,buf + i,1);
          if (!buf[i]) failure.s[failure.len - 1] = 'N';
        }
        buf[len] = 0;
 
        if (relayclient)
          str_copy(buf + len,relayclient);
        else
          switch(rcpthosts(buf,len)) {
            case -1: resources();
            case 0: failure.s[failure.len - 1] = 'D';
          }
 
        if (!failure.s[failure.len - 1]) {
          qmail_to(&qq,buf);
          flagbother = 1;
        }
      }
      getcomma();
      biglen -= (len + 1);
    }
    getcomma();
 
    if (!flagbother) qmail_fail(&qq);
    result = qmail_close(&qq);
    if (!flagsenderok) result = "Dunacceptable sender (#5.1.7)";
    if (databytes) if (!bytestooverflow) result = "Dsorry, that message size exceeds my databytes limit (#5.3.4)";
 
    if (*result)
      len = str_len(result);
    else {
      /* success! */
      len = 0;
      len += fmt_str(buf2 + len,"Kok ");
      len += ulong_fmt(buf2 + len,(unsigned long) now());
      len += fmt_str(buf2 + len," qp ");
      len += ulong_fmt(buf2 + len,qp);
      buf2[len] = 0;
      result = buf2;
    }
      
    len = ulong_fmt(buf,len);
    buf[len++] = ':';
    len += fmt_str(buf + len,result);
    buf[len++] = ',';
 
    for (i = 0;i < failure.len;++i)
      switch(failure.s[i]) {
        case 0:
          buffer_put(&bout,buf,len);
          break;
        case 'D':
          buffer_puts(&bout,"66:Dsorry, that domain isn't in my list of allowed rcpthosts (#5.7.1),");
          break;
        default:
          buffer_puts(&bout,"46:Dsorry, I can't handle that recipient (#5.1.3),");
          break;
      }
 
    /* out will be flushed when we read from the network again */
  }
}
