#include <unistd.h>

#include <skalibs/stralloc.h>
#include <skalibs/buffer.h>

#include "qsutil.h"

static stralloc foo = STRALLOC_ZERO;

static char errbuf[1];
static buffer berr = BUFFER_INIT(buffer_write,0,errbuf,1);

void logsa(stralloc *sa) {
 buffer_putflush(&berr,sa->s,sa->len); }
void log1(char *s1) {
 buffer_putsflush(&berr,s1); }
void qslog2(char *s1, char *s2) {
 buffer_putsflush(&berr,s1);
 buffer_putsflush(&berr,s2); }
void log3(char *s1, char *s2, char *s3) {
 buffer_putsflush(&berr,s1);
 buffer_putsflush(&berr,s2);
 buffer_putsflush(&berr,s3); }
void nomem() { log1("alert: out of memory, sleeping...\n"); sleep(10); }

void pausedir(dir) char *dir;
{ log3("alert: unable to opendir ",dir,", sleeping...\n"); sleep(10); }

static int issafe(ch) char ch;
{
 if (ch == '%') return 0; /* general principle: allman's code is crap */
 if (ch < 33) return 0;
 if (ch > 126) return 0;
 return 1;
}

void logsafe(char *s)
{
 int i;
 while (!stralloc_copys(&foo,s)) nomem();
 for (i = 0;i < foo.len;++i)
   if (foo.s[i] == '\n')
     foo.s[i] = '/';
   else
     if (!issafe(foo.s[i]))
       foo.s[i] = '_';
 logsa(&foo);
}
