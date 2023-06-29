#include <errno.h>

#include <unistd.h>
#include <sys/uio.h>
#include <signal.h>

#include <skalibs/buffer.h>
#include <skalibs/stralloc.h>
#include <skalibs/sig.h>
#include <skalibs/ip46.h>
#include <skalibs/types.h>
#include <skalibs/socket.h>
#include <skalibs/tai.h>
#include <skalibs/djbunix.h>
#include <skalibs/unix-timed.h>

#include "getln.h"
#include "auto_qmail.h"
#include "slurpclose.h"
#include "control.h"

#define PORT_QMQP 628

void die_success(void) { _exit(0); }
void die_perm(void) { _exit(31); }
void nomem(void) { _exit(51); }
void die_read(void) { if (errno == ENOMEM) nomem(); _exit(54); }
void die_control(void) { _exit(55); }
void die_socket(void) { _exit(56); }
void die_home(void) { _exit(61); }
void die_temp(void) { _exit(71); }
void die_conn(void) { _exit(74); }
void die_format(void) { _exit(91); }

int lasterror = 55;
int qmqpfd;

/* TODO are they that safe? */
static ssize_t saferead(int fd, const struct iovec *vbuf, unsigned int n)
{
    ssize_t r = readv(fd, vbuf, n);
    if (r <= 0)
        die_conn();
    return r;
}

static ssize_t safewrite(int fd, const struct iovec *vbuf, unsigned int n)
{
    ssize_t r = writev(fd, vbuf, n);
    if (r <= 0)
        die_conn();
    return r;
}

char buf[1024];
buffer to = BUFFER_INIT(safewrite,-1,buf,sizeof(buf));
buffer from = BUFFER_INIT(saferead,-1,buf,sizeof(buf));
buffer envelope = BUFFER_INIT(buffer_read,1,buf,sizeof(buf));
/* WARNING: can use only one of these at a time! */

stralloc beforemessage = STRALLOC_ZERO;
stralloc message = STRALLOC_ZERO;
stralloc aftermessage = STRALLOC_ZERO;

char strnum[ULONG_FMT];
stralloc line = STRALLOC_ZERO;

void getmess(void)
{
  int match;

  if (slurpclose(0,&message,1024) == -1) die_read();

  strnum[ulong_fmt(strnum,(unsigned long) message.len)] = 0;
  if (!stralloc_copys(&beforemessage,strnum)) nomem();
  if (!stralloc_cats(&beforemessage,":")) nomem();
  if (!stralloc_copys(&aftermessage,",")) nomem();

  if (getln(&envelope,&line,&match,'\0') == -1) die_read();
  if (!match) die_format();
  if (line.len < 2) die_format();
  if (line.s[0] != 'F') die_format();

  strnum[ulong_fmt(strnum,(unsigned long) line.len - 2)] = 0;
  if (!stralloc_cats(&aftermessage,strnum)) nomem();
  if (!stralloc_cats(&aftermessage,":")) nomem();
  if (!stralloc_catb(&aftermessage,line.s + 1,line.len - 2)) nomem();
  if (!stralloc_cats(&aftermessage,",")) nomem();

  for (;;) {
    if (getln(&envelope,&line,&match,'\0') == -1) die_read();
    if (!match) die_format();
    if (line.len < 2) break;
    if (line.s[0] != 'T') die_format();

    strnum[ulong_fmt(strnum,(unsigned long) line.len - 2)] = 0;
    if (!stralloc_cats(&aftermessage,strnum)) nomem();
    if (!stralloc_cats(&aftermessage,":")) nomem();
    if (!stralloc_catb(&aftermessage,line.s + 1,line.len - 2)) nomem();
    if (!stralloc_cats(&aftermessage,",")) nomem();
  }
}

void doit(char *server)
{
  ip46 ip;
  char ch;
  tain now, deadline;

  if (!ip46_scan(server, &ip)) return;

  qmqpfd = socket_tcp4();
  if (qmqpfd == -1) die_socket();

  tain_now(&now);
  tain_addsec(&deadline, &now, 10);
  if (socket_deadlineconnstamp46(qmqpfd, &ip, PORT_QMQP, &deadline, &now) != 0) {
    lasterror = 73;
    if (errno == ETIMEDOUT) lasterror = 72;
    close(qmqpfd);
    return;
  }

  strnum[ulong_fmt(strnum,(unsigned long) (beforemessage.len + message.len + aftermessage.len))] = 0;
  tain_addsec(&deadline, &now, 60);
  if (buffer_timed_puts(&to, strnum, &deadline, &now) <= 0
      || buffer_timed_puts(&to, ":", &deadline, &now) <= 0
      || buffer_timed_put(&to, beforemessage.s,beforemessage.len, &deadline,
        &now) <= 0
      || buffer_timed_put(&to, message.s,message.len, &deadline, &now) <= 0
      || buffer_timed_put(&to, aftermessage.s,aftermessage.len, &deadline,
        &now) <= 0
      || buffer_timed_puts(&to, ",", &deadline, &now) <= 0
      || buffer_timed_flush(&to, &deadline, &now) <= 0)
    die_conn();

  for (;;) {
    tain_addsec(&deadline, &now, 60);
    if (buffer_timed_get(&from, &ch, 1, &deadline, &now) <= 0)
      die_conn();
    if (ch == 'K') die_success();
    if (ch == 'Z') die_temp();
    if (ch == 'D') die_perm();
  }
}

stralloc servers = STRALLOC_ZERO;

int main()
{
  int i;
  int j;

  sig_ignore(SIGPIPE);

  if (chdir(auto_qmail) == -1) die_home();
  if (control_init() == -1) die_control();
  if (control_readfile(&servers,"control/qmqpservers",0) != 1) die_control();

  getmess();

  i = 0;
  for (j = 0;j < servers.len;++j)
    if (!servers.s[j]) {
      doit(servers.s + i);
      i = j + 1;
    }

  return lasterror;
}
