#include <sys/types.h>
#include <sys/time.h>
#include <syslog.h>
#include <unistd.h>

#include <skalibs/buffer.h>
#include <skalibs/bytestr.h>
#include <skalibs/types.h>

/*
#include "error.h"
#include "substdio.h"
#include "subfd.h"
#include "exit.h"
#include "str.h"
#include "scan.h"
#include "fmt.h"
*/

char buf[800]; /* syslog truncates long lines (or crashes); GPACIC */
int bufpos = 0; /* 0 <= bufpos < sizeof(buf) */
int flagcont = 0;
int priority; /* defined if flagcont */
char stamp[ULONG_FMT + ULONG_FMT + 3]; /* defined if flagcont */

void stamp_make(void)
{
  struct timeval tv;
  char *s;
  gettimeofday(&tv,NULL);
  s = stamp;
  s += ulong_fmt(s,(unsigned long) tv.tv_sec);
  *s++ = '.';
  s += uint0_fmt(s,(unsigned int) tv.tv_usec,6);
  *s = 0;
}

void flush(void)
{
  if (bufpos) {
    buf[bufpos] = 0;
    if (flagcont)
      syslog(priority,"%s+%s",stamp,buf); /* logger folds invisibly; GPACIC */
    else {
      stamp_make();
      priority = LOG_INFO;
      if (str_start(buf,"warning:")) priority = LOG_WARNING;
      if (str_start(buf,"alert:")) priority = LOG_ALERT;
      syslog(priority,"%s %s",stamp,buf);
      flagcont = 1;
    }
  }
  bufpos = 0;
}

int main(int argc, char **argv)
{
  char ch;

  if (argc > 1)
    if (argc > 2) {
      unsigned long facility;
      ulong_scan(argv[2],&facility);
      openlog(argv[1],0,facility << 3);
    }
    else
      openlog(argv[1],0,LOG_MAIL);
  else
    openlog("splogger",0,LOG_MAIL);

  for (;;) {
    if (buffer_get(buffer_0,&ch,1) < 1) _exit(0);
    if (ch == '\n') { flush(); flagcont = 0; continue; }
    if (bufpos == sizeof(buf) - 1) flush();
    if ((ch < 32) || (ch > 126)) ch = '?'; /* logger truncates at 0; GPACIC */
    buf[bufpos++] = ch;
  }
}
