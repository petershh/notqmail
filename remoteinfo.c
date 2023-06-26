#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include <skalibs/ip46.h>
#include <skalibs/buffer.h>
#include <skalibs/socket.h>
#include <skalibs/tai.h>
#include <skalibs/types.h>
#include <skalibs/unix-timed.h>
#include <skalibs/djbunix.h>

#include "remoteinfo.h"

/*
#include "byte.h"
#include "substdio.h"
#include "ip.h"
#include "timeoutconn.h"
#include "timeoutread.h"
#include "timeoutwrite.h"
*/

#include "fmt.h"

static char line[999];
static int t;

/*
static ssize_t mywrite(int fd, const void *buf, size_t len)
{
  return timeoutwrite(t,fd,buf,len);
}
static ssize_t myread(int fd, void *buf, size_t len)
{
  return timeoutread(t,fd,buf,len);
}
*/

char *remoteinfo_get(ip46 *ipr, unsigned long rp, ip46 *ipl, unsigned long lp,
        int timeout)
{
  char *x;
  int s;
  buffer b;
  char buf[32];
  unsigned int len;
  int numcolons;
  char ch;
  tain now, deadline;

  t = timeout;
 
  s = socket_tcp4();
  if (s == -1) return 0;
 
  if (socket_bind46(s, ipl, 0) == -1) {
    close(s);
    return 0;
  }
  /* TODO: this api should be deadline-based in the first place */
  tain_now(&now);
  tain_addsec(&deadline, &now, t);
  if (socket_deadlineconnstamp46(s, ipr, 113, &deadline, &now) == -1) {
    close(s);
    return 0;
  }
  ndelay_off(s);
 
  len = 0;
  len += ulong_fmt(line + len,rp);
  len += fmt_str(line + len," , ");
  len += ulong_fmt(line + len,lp);
  len += fmt_str(line + len,"\r\n");
 
  buffer_init(&b,buffer_write,s,buf,sizeof(buf));
  if (buffer_timed_put(&b, line, len, &deadline, &now)) {
    close(s);
    return 0;
  }
  if (buffer_timed_flush(&b, &deadline, &now) != 1) {
    close(s);
    return 0;
  }
 
  buffer_init(&b,buffer_read,s,buf,sizeof(buf));
  x = line;
  numcolons = 0;
  for (;;) {
    tain_addsec(&deadline, &now, t);
    if (buffer_timed_get(&b, &ch, 1, &deadline, &now) != 1) {
      close(s);
      return 0;
    }
    if ((ch == ' ') || (ch == '\t') || (ch == '\r')) continue;
    if (ch == '\n') break;
    if (numcolons < 3) { if (ch == ':') ++numcolons; }
    else { *x++ = ch; if (x == line + sizeof(line) - 1) break; }
  }
  *x = 0;
  close(s);
  return line;
}
