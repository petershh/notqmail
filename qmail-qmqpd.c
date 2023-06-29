#include <unistd.h>
#include <sys/uio.h>
#include <signal.h>

#include <skalibs/sig.h>
#include <skalibs/buffer.h>
#include <skalibs/bytestr.h>
#include <skalibs/types.h>
#include <skalibs/env.h>

#include "fmt.h"
#include "auto_qmail.h"
#include "qmail.h"
#include "received.h"
#include "now.h"

void resources() { _exit(111); }

void resources_handler(int sig) { _exit(111); }

ssize_t safewrite(int fd, const struct iovec *vbuf, unsigned int len)
{
  ssize_t r;
  r = writev(fd,vbuf,len);
  if (r == 0 || r == -1) _exit(0);
  return r;
}
ssize_t saferead(int fd, const struct iovec *vbuf, unsigned int len)
{
  ssize_t r;
  r = readv(fd,vbuf,len);
  if (r == 0 || r == -1) _exit(0);
  return r;
}

char binbuf[512];
buffer bin = BUFFER_INIT(saferead,0,binbuf,sizeof(binbuf));
char boutbuf[256];
buffer bout = BUFFER_INIT(safewrite,1,boutbuf,sizeof(boutbuf));

unsigned long bytesleft = 100;

void getbyte(char *ch)
{
  if (!bytesleft--) _exit(100);
  buffer_get(&bin,ch,1);
}

unsigned long getlen(void)
{
  unsigned long len = 0;
  char ch;

  for (;;) {
    getbyte(&ch);
    if (ch == ':') return len;
    if (len > 200000000) resources();
    len = 10 * len + (ch - '0');
  }
}

void getcomma(void)
{
  char ch;
  getbyte(&ch);
  if (ch != ',') _exit(100);
}

struct qmail qq;

void identify(void)
{
  char const *remotehost;
  char const *remoteinfo;
  char const *remoteip;
  char const *local;

  remotehost = env_get("TCPREMOTEHOST");
  if (!remotehost) remotehost = "unknown";
  remoteinfo = env_get("TCPREMOTEINFO");
  remoteip = env_get("TCPREMOTEIP");
  if (!remoteip) remoteip = "unknown";
  local = env_get("TCPLOCALHOST");
  if (!local) local = env_get("TCPLOCALIP");
  if (!local) local = "unknown";
 
  received(&qq,"QMQP",local,remoteip,remotehost,remoteinfo,NULL);
}

char buf[1000];
char strnum[ULONG_FMT];

int getbuf(void)
{
  unsigned long len;
  int i;

  len = getlen();
  if (len >= 1000) {
    for (i = 0;i < len;++i) getbyte(buf);
    getcomma();
    buf[0] = 0;
    return 0;
  }

  for (i = 0;i < len;++i) getbyte(buf + i);
  getcomma();
  buf[len] = 0;
  return byte_chr(buf,len,'\0') == len;
}

int flagok = 1;

int
main(void)
{
  char *result;
  unsigned long qp;
  unsigned long len;
  char ch;

  sig_ignore(SIGPIPE);
  sig_catch(SIGALRM, resources_handler);
  alarm(3600);

  bytesleft = getlen();

  len = getlen();

  if (chdir(auto_qmail) == -1) resources();
  if (qmail_open(&qq) == -1) resources();
  qp = qmail_qp(&qq);
  identify();

  while (len > 0) { /* XXX: could speed this up */
    getbyte(&ch);
    --len;
    qmail_put(&qq,&ch,1);
  }
  getcomma();

  if (getbuf())
    qmail_from(&qq,buf);
  else {
    qmail_from(&qq,"");
    qmail_fail(&qq);
    flagok = 0;
  }

  while (bytesleft)
    if (getbuf())
      qmail_to(&qq,buf);
    else {
      qmail_fail(&qq);
      flagok = 0;
    }

  bytesleft = 1;
  getcomma();

  result = qmail_close(&qq);

  if (!*result) {
    len = fmt_str(buf,"Kok ");
    len += ulong_fmt(buf + len,(unsigned long) now());
    len += fmt_str(buf + len," qp ");
    len += ulong_fmt(buf + len,qp);
    buf[len] = 0;
    result = buf;
  }

  if (!flagok)
    result = "Dsorry, I can't accept addresses like that (#5.1.3)";

  buffer_put(&bout,strnum,ulong_fmt(strnum,(unsigned long) str_len(result)));
  buffer_puts(&bout,":");
  buffer_puts(&bout,result);
  buffer_puts(&bout,",");
  buffer_flush(&bout);
  _exit(0);
}
