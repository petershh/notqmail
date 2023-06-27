#include <errno.h>

#include <unistd.h>
#include <sys/uio.h>

#include <skalibs/sig.h>
#include <skalibs/stralloc.h>
#include <skalibs/buffer.h>
#include <skalibs/alloc.h>
#include <skalibs/tai.h>
#include <skalibs/bytestr.h>
#include <skalibs/types.h>
#include <skalibs/env.h>
#include <skalibs/ip46.h>
#include <skalibs/unix-timed.h>

/*
#include "sig.h"
#include "readwrite.h"
#include "stralloc.h"
#include "substdio.h"
#include "alloc.h"
#include "datetime.h"
#include "error.h"
#include "ip.h"
#include "str.h"
#include "fmt.h"
#include "scan.h"
#include "byte.h"
#include "case.h"
#include "env.h"
#include "now.h"
#include "exit.h"
#include "timeoutread.h"
#include "timeoutwrite.h"
*/

#include "auto_qmail.h"
#include "control.h"
#include "received.h"
#include "constmap.h"
#include "ipme.h"
#include "qmail.h"
#include "rcpthosts.h"
#include "commands.h"

#define MAXHOPS 100
unsigned int databytes = 0;
int timeout = 1200;

/*
GEN_SAFE_TIMEOUTWRITE(safewrite,timeout,fd,_exit(1))
*/

ssize_t safewrite(int fd, const struct iovec *vbuf, unsigned int n) {
    ssize_t r = writev(fd, vbuf, n);
    if (r <= 0)
        _exit(1);
    return r;
}

char boutbuf[512];
buffer bout = BUFFER_INIT(safewrite,1,boutbuf,sizeof(boutbuf));

void flush(void) {
  int r;
  tain now, deadline;
  tain_now(&now);
  tain_addsec(&deadline, &now, timeout);
  r = buffer_timed_flush(&bout, &deadline, &now);
  if (r <= 0 && errno == ETIMEDOUT)
    _exit(1);
}
void out(char *s) {
  int r;
  tain now, deadline;
  tain_now(&now);
  tain_addsec(&deadline, &now, timeout);
  r = buffer_timed_puts(&bout, s, &deadline, &now);
  if (r <= 0 && errno == ETIMEDOUT)
    _exit(1);
}

void die_read(void) { _exit(1); }
void die_alarm(void) { out("451 timeout (#4.4.2)\r\n"); flush(); _exit(1); }
void die_nomem(void) { out("421 out of memory (#4.3.0)\r\n"); flush(); _exit(1); }
void die_control(void) { out("421 unable to read controls (#4.3.0)\r\n"); flush(); _exit(1); }
void die_ipme(void) { out("421 unable to figure out my IP addresses (#4.3.0)\r\n"); flush(); _exit(1); }
void straynewline(void) { out("451 See https://cr.yp.to/docs/smtplf.html.\r\n"); flush(); _exit(1); }

void err_bmf(void) { out("553 sorry, your envelope sender is in my badmailfrom list (#5.7.1)\r\n"); }
void err_nogateway(void) { out("553 sorry, that domain isn't in my list of allowed rcpthosts (#5.7.1)\r\n"); }
void err_unimpl(char *arg) { out("502 unimplemented (#5.5.1)\r\n"); }
void err_syntax(void) { out("555 syntax error (#5.5.4)\r\n"); }
void err_wantmail(void) { out("503 MAIL first (#5.5.1)\r\n"); }
void err_wantrcpt(void) { out("503 RCPT first (#5.5.1)\r\n"); }
void err_noop(char *arg) { out("250 ok\r\n"); }
void err_vrfy(char *arg) { out("252 send some mail, i'll try my best\r\n"); }
void err_qqt(void) { out("451 qqt failure (#4.3.0)\r\n"); }


stralloc greeting = STRALLOC_ZERO;

void smtp_greet(char *code)
{
  int r;
  tain now, deadline;
  tain_now(&now);
  tain_addsec(&deadline, &now, timeout);
  r = buffer_timed_puts(&bout, code, &deadline, &now);
  if (r <= 0 && errno == ETIMEDOUT)
    _exit(1);
  
  tain_addsec(&deadline, &now, timeout);
  r = buffer_timed_put(&bout,greeting.s,greeting.len, &deadline, &now);
  if (r <= 0 && errno == ETIMEDOUT)
    _exit(1);
}
void smtp_help(char *arg)
{
  out("214 notqmail home page: https://notqmail.org\r\n");
}
void smtp_quit(char *arg)
{
  smtp_greet("221 "); out("\r\n"); flush(); _exit(0);
}

char const *remoteip;
char const *remotehost;
char const *remoteinfo;
char const *local;
char const *relayclient;

stralloc helohost = STRALLOC_ZERO;
char *fakehelo; /* pointer into helohost, or 0 */

void dohelo(char const *arg) {
  if (!stralloc_copys(&helohost,arg)) die_nomem(); 
  if (!stralloc_0(&helohost)) die_nomem(); 
  fakehelo = case_diffs(remotehost,helohost.s) ? helohost.s : 0;
}

int liphostok = 0;
stralloc liphost = STRALLOC_ZERO;
int bmfok = 0;
stralloc bmf = STRALLOC_ZERO;
struct constmap mapbmf;

void setup(void)
{
  char const *x;
  unsigned long u;
 
  if (control_init() == -1) die_control();
  if (control_rldef(&greeting,"control/smtpgreeting",1,NULL) != 1)
    die_control();
  liphostok = control_rldef(&liphost,"control/localiphost",1,NULL);
  if (liphostok == -1) die_control();
  if (control_readint(&timeout,"control/timeoutsmtpd") == -1) die_control();
  if (timeout <= 0) timeout = 1;

  if (rcpthosts_init() == -1) die_control();

  bmfok = control_readfile(&bmf,"control/badmailfrom",0);
  if (bmfok == -1) die_control();
  if (bmfok)
    if (!constmap_init(&mapbmf,bmf.s,bmf.len,0)) die_nomem();
 
  if (control_readint(&databytes,"control/databytes") == -1) die_control();
  x = env_get("DATABYTES");
  if (x) { ulong_scan(x,&u); databytes = u; }
  if (!(databytes + 1)) --databytes;
 
  remoteip = env_get("TCPREMOTEIP");
  if (!remoteip) remoteip = "unknown";
  local = env_get("TCPLOCALHOST");
  if (!local) local = env_get("TCPLOCALIP");
  if (!local) local = "unknown";
  remotehost = env_get("TCPREMOTEHOST");
  if (!remotehost) remotehost = "unknown";
  remoteinfo = env_get("TCPREMOTEINFO");
  relayclient = env_get("RELAYCLIENT");
  dohelo(remotehost);
}

static unsigned int ip_scanbracket(char const *s, ip46 *ip)
{
  unsigned int len;
 
  if (*s != '[') return 0;
  len = ip46_scan(s + 1,ip);
  if (!len) return 0;
  if (s[len + 1] != ']') return 0;
  return len + 2;
}

stralloc addr = STRALLOC_ZERO; /* will be 0-terminated, if addrparse returns 1 */

int addrparse(char *arg)
{
  int i;
  char ch;
  char terminator;
  ip46 ip;
  int flagesc;
  int flagquoted;
 
  terminator = '>';
  i = str_chr(arg,'<');
  if (arg[i])
    arg += i + 1;
  else { /* partner should go read rfc 821 */
    terminator = ' ';
    arg += str_chr(arg,':');
    if (*arg == ':') ++arg;
    while (*arg == ' ') ++arg;
  }

  /* strip source route */
  if (*arg == '@') while (*arg) if (*arg++ == ':') break;

  if (!stralloc_copys(&addr,"")) die_nomem();
  flagesc = 0;
  flagquoted = 0;
  for (i = 0;(ch = arg[i]);++i) { /* copy arg to addr, stripping quotes */
    if (flagesc) {
      if (!stralloc_append(&addr,ch)) die_nomem();
      flagesc = 0;
    }
    else {
      if (!flagquoted && (ch == terminator)) break;
      switch(ch) {
        case '\\': flagesc = 1; break;
        case '"': flagquoted = !flagquoted; break;
        default: if (!stralloc_append(&addr,ch)) die_nomem();
      }
    }
  }
  /* could check for termination failure here, but why bother? */
  if (!stralloc_0(&addr)) die_nomem();

  if (liphostok) {
    i = byte_rchr(addr.s,addr.len,'@');
    if (i < addr.len) /* if not, partner should go read rfc 821 */
      if (addr.s[i + 1] == '[')
        if (!addr.s[i + 1 + ip_scanbracket(addr.s + i + 1,&ip)])
          if (ipme_is(&ip)) {
            addr.len = i + 1;
            if (!stralloc_cat(&addr,&liphost)) die_nomem();
            if (!stralloc_0(&addr)) die_nomem();
          }
  }

  if (addr.len > 900) return 0;
  return 1;
}

int bmfcheck(void)
{
  int j;
  if (!bmfok) return 0;
  if (constmap(&mapbmf,addr.s,addr.len - 1)) return 1;
  j = byte_rchr(addr.s,addr.len,'@');
  if (j < addr.len)
    if (constmap(&mapbmf,addr.s + j,addr.len - j - 1)) return 1;
  return 0;
}

int addrallowed(void)
{
  int r;
  r = rcpthosts(addr.s,str_len(addr.s));
  if (r == -1) die_control();
  return r;
}


int seenmail = 0;
int flagbarf; /* defined if seenmail */
stralloc mailfrom = STRALLOC_ZERO;
stralloc rcptto = STRALLOC_ZERO;

void smtp_helo(char *arg)
{
  smtp_greet("250 "); out("\r\n");
  seenmail = 0; dohelo(arg);
}
void smtp_ehlo(char *arg)
{
  smtp_greet("250-"); out("\r\n250-PIPELINING\r\n250 8BITMIME\r\n");
  seenmail = 0; dohelo(arg);
}
void smtp_rset(char *arg)
{
  seenmail = 0;
  out("250 flushed\r\n");
}
void smtp_mail(char *arg)
{
  if (!addrparse(arg)) { err_syntax(); return; }
  flagbarf = bmfcheck();
  seenmail = 1;
  if (!stralloc_copys(&rcptto,"")) die_nomem();
  if (!stralloc_copys(&mailfrom,addr.s)) die_nomem();
  if (!stralloc_0(&mailfrom)) die_nomem();
  out("250 ok\r\n");
}
void smtp_rcpt(char *arg) {
  if (!seenmail) { err_wantmail(); return; }
  if (!addrparse(arg)) { err_syntax(); return; }
  if (flagbarf) { err_bmf(); return; }
  if (relayclient) {
    --addr.len;
    if (!stralloc_cats(&addr,relayclient)) die_nomem();
    if (!stralloc_0(&addr)) die_nomem();
  }
  else
    if (!addrallowed()) { err_nogateway(); return; }
  if (!stralloc_cats(&rcptto,"T")) die_nomem();
  if (!stralloc_cats(&rcptto,addr.s)) die_nomem();
  if (!stralloc_0(&rcptto)) die_nomem();
  out("250 ok\r\n");
}

ssize_t saferead(int fd, struct iovec const *buf, unsigned int len)
{
  ssize_t r;
  flush();
  r = readv(fd, buf, len);
  if (r == 0 || r == -1) die_read();
  return r;
}

char binbuf[1024];
buffer bin = BUFFER_INIT(saferead,0,binbuf,sizeof(binbuf));

struct qmail qqt;
unsigned int bytestooverflow = 0;

void put(char *ch)
{
  if (bytestooverflow)
    if (!--bytestooverflow)
      qmail_fail(&qqt);
  qmail_put(&qqt,ch,1);
}

void blast(int *hops)
{
  char ch;
  int state;
  int flaginheader;
  int pos; /* number of bytes since most recent \n, if fih */
  int flagmaybex; /* 1 if this line might match RECEIVED, if fih */
  int flagmaybey; /* 1 if this line might match \r\n, if fih */
  int flagmaybez; /* 1 if this line might match DELIVERED, if fih */
  int r;
  tain deadline, now;
 
  state = 1;
  *hops = 0;
  flaginheader = 1;
  pos = 0; flagmaybex = flagmaybey = flagmaybez = 1;
  for (;;) {
    tain_now(&now);
    tain_addsec(&deadline, &now, timeout);
    r = buffer_timed_get(&bin, &ch, 1, &deadline, &now);
    if (r <= 0 && errno == ETIMEDOUT) die_read();
    if (flaginheader) {
      if (pos < 9) {
        if (ch != "delivered"[pos]) if (ch != "DELIVERED"[pos]) flagmaybez = 0;
        if (flagmaybez) if (pos == 8) ++*hops;
        if (pos < 8)
          if (ch != "received"[pos]) if (ch != "RECEIVED"[pos]) flagmaybex = 0;
        if (flagmaybex) if (pos == 7) ++*hops;
        if (pos < 2) if (ch != "\r\n"[pos]) flagmaybey = 0;
        if (flagmaybey) if (pos == 1) flaginheader = 0;
	++pos;
      }
      if (ch == '\n') { pos = 0; flagmaybex = flagmaybey = flagmaybez = 1; }
    }
    switch(state) {
      case 0:
        if (ch == '\n') straynewline();
        if (ch == '\r') { state = 4; continue; }
        break;
      case 1: /* \r\n */
        if (ch == '\n') straynewline();
        if (ch == '.') { state = 2; continue; }
        if (ch == '\r') { state = 4; continue; }
        state = 0;
        break;
      case 2: /* \r\n + . */
        if (ch == '\n') straynewline();
        if (ch == '\r') { state = 3; continue; }
        state = 0;
        break;
      case 3: /* \r\n + .\r */
        if (ch == '\n') return;
        put(".");
        put("\r");
        if (ch == '\r') { state = 4; continue; }
        state = 0;
        break;
      case 4: /* + \r */
        if (ch == '\n') { state = 1; break; }
        if (ch != '\r') { put("\r"); state = 0; }
    }
    put(&ch);
  }
}

char accept_buf[TAIN_FMT];
void acceptmessage(unsigned long qp)
{
  tain when;
  tain_now(&when);
  out("250 ok ");
  accept_buf[tain_fmt(accept_buf, &when)] = 0;
  out(accept_buf);
  out(" qp ");
  accept_buf[ulong_fmt(accept_buf,qp)] = 0;
  out(accept_buf);
  out("\r\n");
}

void smtp_data(char *arg) {
  int hops;
  unsigned long qp;
  char *qqx;
 
  if (!seenmail) { err_wantmail(); return; }
  if (!rcptto.len) { err_wantrcpt(); return; }
  seenmail = 0;
  if (databytes) bytestooverflow = databytes + 1;
  if (qmail_open(&qqt) == -1) { err_qqt(); return; }
  qp = qmail_qp(&qqt);
  out("354 go ahead\r\n");
 
  received(&qqt,"SMTP",local,remoteip,remotehost,remoteinfo,fakehelo);
  blast(&hops);
  hops = (hops >= MAXHOPS);
  if (hops) qmail_fail(&qqt);
  qmail_from(&qqt,mailfrom.s);
  qmail_put(&qqt,rcptto.s,rcptto.len);
 
  qqx = qmail_close(&qqt);
  if (!*qqx) { acceptmessage(qp); return; }
  if (hops) { out("554 too many hops, this message is looping (#5.4.6)\r\n"); return; }
  if (databytes) if (!bytestooverflow) { out("552 sorry, that message size exceeds my databytes limit (#5.3.4)\r\n"); return; }
  if (*qqx == 'D') out("554 "); else out("451 ");
  out(qqx + 1);
  out("\r\n");
}

struct commands smtpcommands[] = {
  { "rcpt", smtp_rcpt, 0 }
, { "mail", smtp_mail, 0 }
, { "data", smtp_data, flush }
, { "quit", smtp_quit, flush }
, { "helo", smtp_helo, flush }
, { "ehlo", smtp_ehlo, flush }
, { "rset", smtp_rset, 0 }
, { "help", smtp_help, flush }
, { "noop", err_noop, flush }
, { "vrfy", err_vrfy, flush }
, { 0, err_unimpl, flush }
} ;

int main(void)
{
  sig_ignore(SIGPIPE);
  if (chdir(auto_qmail) == -1) die_control();
  setup();
  if (ipme_init() != 1) die_ipme();
  smtp_greet("220 ");
  out(" ESMTP\r\n");
  if (commands(&bin,&smtpcommands) == 0) die_read();
  die_nomem();
}
