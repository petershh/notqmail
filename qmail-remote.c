#include <errno.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/uio.h>

#include <skalibs/sig.h>
#include <skalibs/stralloc.h>
#include <skalibs/buffer.h>
#include <skalibs/types.h>
#include <skalibs/bytestr.h>
#include <skalibs/ip46.h>
#include <skalibs/genalloc.h>
#include <skalibs/tai.h>
#include <skalibs/socket.h>
#include <skalibs/unix-timed.h>

#include <s6-dns/s6dns.h>

#include "auto_qmail.h"
#include "quote.h"
#include "control.h"
#include "ipme.h"
#include "constmap.h"
#include "noreturn.h"
#include "tcpto.h"
#include "ipalloc.h"

#define HUGESMTPTEXT 5000

#define PORT_SMTP 25 /* silly rabbit, /etc/services is for users */
unsigned long port = PORT_SMTP;

static stralloc sauninit = STRALLOC_ZERO;

stralloc helohost = STRALLOC_ZERO;
stralloc routes = STRALLOC_ZERO;
struct constmap maproutes;
stralloc host = STRALLOC_ZERO;
stralloc sender = STRALLOC_ZERO;

genalloc reciplist = GENALLOC_ZERO;

ip46 partner;

void out(char *s) { if (buffer_puts(buffer_1small,s) == -1) _exit(0); }
void zero(void) { if (buffer_put(buffer_1small,"\0",1) == -1) _exit(0); }
void _noreturn_ zerodie(void) { zero(); buffer_flush(buffer_1small); _exit(0); }

void outsafe(stralloc *sa)
{
  int i; char ch;
  for (i = 0; i < sa->len; ++i) {
    ch = sa->s[i];
    if (ch < 33)
      ch = '?';
    if (ch > 126)
      ch = '?';
    if (buffer_put(buffer_1small,&ch,1) == -1)
      _exit(0);
  }
}

void _noreturn_ temp_nomem(void)
{
  out("ZOut of memory. (#4.3.0)\n");
  zerodie();
}

void _noreturn_ temp_oserr(void)
{
  out("ZSystem resources temporarily unavailable. (#4.3.0)\n");
  zerodie();
}

void _noreturn_ temp_noconn(void)
{
  out("ZSorry, I wasn't able to establish an SMTP connection. (#4.4.1)\n");
  zerodie();
}

void _noreturn_ temp_read(void)
{
  out("ZUnable to read message. (#4.3.0)\n");
  zerodie();
}

void _noreturn_ temp_dnscanon(void)
{
  out("ZCNAME lookup failed temporarily. (#4.4.3)\n");
  zerodie();
}

void _noreturn_ temp_dns(void)
{
  out("ZSorry, I couldn't find any host by that name. (#4.1.2)\n");
  zerodie();
}

void _noreturn_ temp_chdir(void)
{
  out("ZUnable to switch to home directory. (#4.3.0)\n");
  zerodie();
}

void _noreturn_ temp_control(void)
{
  out("ZUnable to read control files. (#4.3.0)\n");
  zerodie();
}

void _noreturn_ perm_partialline(void)
{
  out("DSMTP cannot transfer messages with partial final lines. (#5.6.2)\n");
  zerodie();
}

void _noreturn_ perm_usage(void)
{
  out("DI (qmail-remote) was invoked improperly. (#5.3.5)\n");
  zerodie();
}

void _noreturn_ perm_dns(void) {
  out("DSorry, I couldn't find any host named ");
  outsafe(&host);
  out(". (#5.1.2)\n");
  zerodie();
}

void _noreturn_ perm_nomx(void)
{
  out("DSorry, I couldn't find a mail exchanger or IP address. (#5.4.4)\n");
  zerodie();
}

void _noreturn_ perm_ambigmx(void)
{
  out("DSorry. Although I'm listed as a best-preference MX or A for that host,\
\nit isn't in my control/locals file, so I don't treat it as local. (#5.4.6)\n");
  zerodie();
}

void outhost(void)
{
  char x[IP46_FMT];
  if (buffer_put(buffer_1small,x,ip46_fmt(x,&partner)) == -1) _exit(0);
}

int flagcritical = 0;

void _noreturn_ dropped(void) {
  out("ZConnected to ");
  outhost();
  out(" but connection died. ");
  if (flagcritical) out("Possible duplicate! ");
  out("(#4.4.2)\n");
  zerodie();
}

int timeoutconnect = 60;
int smtpfd;
int timeout = 1200;

ssize_t saferead(int fd, struct iovec const *vbuf, unsigned int n)
{
    ssize_t r = readv(fd, vbuf, n);
    if (r <= 0)
        dropped();
    return r;
}

ssize_t safewrite(int fd, struct iovec const *vbuf, unsigned int n)
{
    ssize_t r = writev(fd, vbuf, n);
    if (r <= 0)
        dropped();
    return r;
}

char inbuf[1024];
buffer bin = BUFFER_INIT(buffer_read,0,inbuf,sizeof(inbuf));
char smtptobuf[1024];
buffer smtpto = BUFFER_INIT(safewrite,-1,smtptobuf,sizeof(smtptobuf));
char smtpfrombuf[128];
buffer smtpfrom = BUFFER_INIT(saferead,-1,smtpfrombuf,sizeof(smtpfrombuf));

stralloc smtptext = STRALLOC_ZERO;

static void get(unsigned char *uc)
{
  tain now;
  tain deadline = TAIN_ZERO;
  char *ch = (char *)uc;
  tain_now(&now);
  tain_addsec(&deadline, &now, timeout);
  if (buffer_timed_get(&smtpfrom, ch, 1, &deadline, &now) <= 0)
    dropped();
  if (*ch != '\r')
    if (smtptext.len < HUGESMTPTEXT)
     if (!stralloc_append(&smtptext, *ch)) temp_nomem();
}

unsigned long smtpcode(void)
{
  unsigned char ch;
  unsigned long code;

  if (!stralloc_copys(&smtptext,"")) temp_nomem();

  get(&ch); code = ch - '0';
  get(&ch); code = code * 10 + (ch - '0');
  get(&ch); code = code * 10 + (ch - '0');
  for (;;) {
    get(&ch);
    if (ch != '-') break;
    while (ch != '\n') get(&ch);
    get(&ch);
    get(&ch);
    get(&ch);
  }
  while (ch != '\n') get(&ch);

  return code;
}

void outsmtptext(void)
{
  int i; 
  if (smtptext.s) if (smtptext.len) {
    out("Remote host said: ");
    for (i = 0;i < smtptext.len;++i)
      if (!smtptext.s[i]) smtptext.s[i] = '?';
    if (buffer_put(buffer_1small, smtptext.s, smtptext.len) == -1) _exit(0);
    smtptext.len = 0;
  }
}

void quit(char *prepend, char *append)
{
  tain now, deadline = TAIN_ZERO;
  tain_now(&now);
  tain_addsec(&deadline, &now, timeout);
  if (buffer_timed_puts(&smtpto, "QUIT\r\n", &deadline, &now) <= 0)
    dropped();
  if (buffer_timed_flush(&smtpto, &deadline, &now) <= 0)
    dropped();
  /* waiting for remote side is just too ridiculous */
  out(prepend);
  outhost();
  out(append);
  out(".\n");
  outsmtptext();
  zerodie();
}

void blast(void)
{
  int r;
  char ch;
  tain now = TAIN_ZERO;
  tain deadline = TAIN_ZERO;

  for (;;) {
    r = buffer_get(&bin,&ch,1);
    if (r == 0) break;
    if (r == -1) temp_read();
    if (ch == '.') {
      tain_now(&now);
      tain_addsec(&deadline, &now, timeout);
      if (buffer_timed_put(&smtpto, ".", 1, &deadline, &now) <= 0)
        dropped();
    }
    while (ch != '\n') {
      if (ch == '\r') {
        r = buffer_get(&bin, &ch, 1);
        if (r == 0)
          break;
        if (r == -1) temp_read();
        if (ch != '\n') {
          tain_now(&now);
          tain_addsec(&deadline, &now, timeout);
          if (buffer_timed_put(&smtpto, "\r\n", 2, &deadline, &now) <= 0)
              dropped();
        } else
          break;
      }
      tain_now(&now);
      tain_addsec(&deadline, &now, timeout);
      if (buffer_timed_put(&smtpto, &ch, 1, &deadline, &now) <= 0)
        dropped();
      r = buffer_get(&bin,&ch,1);
      if (r == 0) perm_partialline();
      if (r == -1) temp_read();
    }
    tain_now(&now);
    tain_addsec(&deadline, &now, timeout);
    if (buffer_timed_put(&smtpto, "\r\n", 2, &deadline, &now) <= 0)
      dropped();
  }
 
  flagcritical = 1;
  tain_addsec(&deadline, &now, timeout);
  if (buffer_timed_put(&smtpto, ".\r\n", 3, &deadline, &now) <= 0)
    dropped();
  tain_addsec(&deadline, &now, timeout);
  if (buffer_timed_flush(&smtpto, &deadline, &now) <= 0)
    dropped();
}

stralloc recip = STRALLOC_ZERO;

void smtp(void)
{
  unsigned long code;
  int flagbother;
  int i;
  tain now, deadline = TAIN_ZERO;
 
  if (smtpcode() != 220) quit("ZConnected to "," but greeting failed");

  tain_now(&now);
  tain_addsec(&deadline, &now, timeout);
  if (buffer_timed_puts(&smtpto, "HELO ", &deadline, &now) <= 0
      || buffer_timed_put(&smtpto, helohost.s, helohost.len, &deadline,
        &now) <= 0
      || buffer_timed_puts(&smtpto, "\r\n", &deadline, &now) <= 0
      || buffer_timed_flush(&smtpto, &deadline, &now) <= 0)
    dropped();

  if (smtpcode() != 250) quit("ZConnected to "," but my name was rejected");
 
  tain_now(&now);
  tain_addsec(&deadline, &now, timeout);
  if (buffer_timed_puts(&smtpto, "MAIL FROM:<", &deadline, &now) <= 0
      || buffer_timed_put(&smtpto, sender.s, sender.len, &deadline, &now) <= 0
      || buffer_timed_puts(&smtpto, ">\r\n", &deadline, &now) <= 0
      || buffer_timed_flush(&smtpto, &deadline, &now) <= 0)
    dropped();

  code = smtpcode();
  if (code >= 500) quit("DConnected to "," but sender was rejected");
  if (code >= 400) quit("ZConnected to "," but sender was rejected");
 
  flagbother = 0;
  for (i = 0;i < reciplist.len;++i) {
    tain_now(&now);
    tain_addsec(&deadline, &now, timeout);
    if (buffer_timed_puts(&smtpto, "RCPT TO:<", &deadline, &now)
        || buffer_timed_put(&smtpto, genalloc_s(stralloc, &reciplist)[i].s,
            genalloc_s(stralloc, &reciplist)[i].len, &deadline, &now)
        || buffer_timed_puts(&smtpto, ">\r\n", &deadline, &now)
        || buffer_timed_flush(&smtpto, &deadline, &now))
      dropped();
    code = smtpcode();
    if (code >= 500) {
      out("h"); outhost(); out(" does not like recipient.\n");
      outsmtptext(); zero();
    }
    else if (code >= 400) {
      out("s"); outhost(); out(" does not like recipient.\n");
      outsmtptext(); zero();
    }
    else {
      out("r"); zero();
      flagbother = 1;
    }
  }
  if (!flagbother) quit("DGiving up on ","");
 
  tain_now(&now);
  tain_addsec(&deadline, &now, timeout);
  if (buffer_timed_puts(&smtpto, "DATA\r\n", &deadline, &now) <= 0
      || buffer_timed_flush(&smtpto, &deadline, &now) <= 0)
    dropped();
  code = smtpcode();
  if (code >= 500) quit("D"," failed on DATA command");
  if (code >= 400) quit("Z"," failed on DATA command");
 
  blast();
  code = smtpcode();
  flagcritical = 0;
  if (code >= 500) quit("D"," failed after I sent the message");
  if (code >= 400) quit("Z"," failed after I sent the message");
  quit("K"," accepted message");
}

stralloc canonhost = STRALLOC_ZERO;
stralloc canonbox = STRALLOC_ZERO;

void addrmangle(stralloc *saout, char *s)
{
  int j;
 
  j = str_rchr(s,'@');
  if (!s[j]) {
    if (!stralloc_copys(saout,s)) temp_nomem();
    return;
  }
  if (!stralloc_copys(&canonbox,s)) temp_nomem();
  canonbox.len = j;
  /* box has to be quoted */
  if (!quote(saout,&canonbox)) temp_nomem();
  if (!stralloc_cats(saout,"@")) temp_nomem();
 
  if (!stralloc_copys(&canonhost,s + j + 1)) temp_nomem();

  if (!stralloc_cat(saout,&canonhost)) temp_nomem();
}

void getcontrols(void)
{
  if (control_init() == -1) temp_control();
  if (control_readint(&timeout,"control/timeoutremote") == -1) temp_control();
  if (control_readint(&timeoutconnect,"control/timeoutconnect") == -1)
    temp_control();
  if (control_rldef(&helohost,"control/helohost",1,NULL) != 1)
    temp_control();
  switch(control_readfile(&routes,"control/smtproutes",0)) {
    case -1:
      temp_control();
    case 0:
      if (!constmap_init(&maproutes,"",0,1)) temp_nomem(); break;
    case 1:
      if (!constmap_init(&maproutes,routes.s,routes.len,1)) temp_nomem(); break;
  }
}

/* TODO: make it IPv6 ready once other parts prepared for that
 * should be easy by replacing A query by AAAAA query */
/* TODO: use proper timeouts, we cannot afford blocking */ 
static void dns_ips_with_pref(genalloc *ix, char const *hostname,
    size_t hostname_len, int pref)
{
  int i;
  stralloc ips = STRALLOC_ZERO;
  if (!s6dns_resolve_a_g(&ips, hostname, hostname_len, 0, &tain_infinite))
    temp_dns();
  if (!genalloc_readyplus(struct ip_mx, ix, ips.len / 4))
    temp_nomem();
  for (i = 0; i < ips.len / 4; i++) {
    struct ip_mx temp = IP_MX_ZERO;
    temp.pref = pref;
    temp.ip.is6 = 0;
    memcpy(temp.ip.ip, ips.s + 4 * i, 4);
    genalloc_append(struct ip_mx, ix, &temp);
  }
}

int main(int argc, char **argv)
{
  genalloc ip = GENALLOC_ZERO;
  int i;
  tain now, deadline;
  unsigned long random;
  char **recips;
  unsigned long prefme;
  char *relayhost;

  sig_ignore(SIGPIPE);
  if (argc < 4) perm_usage();
  if (chdir(auto_qmail) == -1) temp_chdir();
  getcontrols();

  if (!s6dns_init()) temp_dns(); /* TODO report properly */

  if (!stralloc_copys(&host,argv[1])) temp_nomem();

  relayhost = 0;
  for (i = 0;i <= host.len;++i)
    if ((i == 0) || (i == host.len) || (host.s[i] == '.'))
      if ((relayhost = constmap(&maproutes,host.s + i,host.len - i)))
        break;
  if (relayhost && !*relayhost) relayhost = 0;

  if (relayhost) {
    i = str_chr(relayhost,':');
    if (relayhost[i]) {
      ulong_scan(relayhost + i + 1,&port);
      relayhost[i] = 0;
    }
    if (!stralloc_copys(&host,relayhost)) temp_nomem();
  }

  addrmangle(&sender,argv[2]);

  if (!genalloc_readyplus(stralloc, &reciplist,0)) temp_nomem();
  if (ipme_init() != 1) temp_oserr();

  recips = argv + 3;
  while (*recips) {
    if (!genalloc_readyplus(stralloc, &reciplist,1)) temp_nomem();
    genalloc_s(stralloc, &reciplist)[genalloc_len(stralloc,
        &reciplist)] = sauninit;
    addrmangle(genalloc_s(stralloc, &reciplist)
        + genalloc_len(stralloc, &reciplist), *recips);
    genalloc_setlen(stralloc, &reciplist,
        genalloc_len(stralloc, &reciplist) + 1);
    ++recips;
  }

  if (relayhost) {
    dns_ips_with_pref(&ip, host.s, host.len, 0);
  } else {
    char mx_name[255]; /* rfc 1035 2.3.4 */
    size_t mx_name_len;
    s6dns_message_rr_mx_t *rrs_arr;
    genalloc mx_rrs;
    if (!s6dns_resolve_mx_g(&mx_rrs, host.s, host.len, 0, NULL))
      temp_dns();
    rrs_arr = genalloc_s(s6dns_message_rr_mx_t, &mx_rrs);
    for (i = 0; i < genalloc_len(s6dns_message_rr_mx_t, &mx_rrs); i++) {
      mx_name_len = s6dns_domain_tostring(mx_name, S6DNS_FMT_MX,
          &rrs_arr[i].exchange);
      if (mx_name_len == 0)
        temp_dns(); /* XXX easy escape, eh? */
      dns_ips_with_pref(&ip, mx_name, mx_name_len, rrs_arr[i].preference);
    }
  }
  s6dns_finish();

  if (genalloc_len(struct ip_mx, &ip) <= 0) perm_nomx();

  prefme = 100000;
  for (i = 0; i < genalloc_len(struct ip_mx, &ip); ++i)
    if (ipme_is(&genalloc_s(struct ip_mx, &ip)[i].ip))
      if (genalloc_s(struct ip_mx, &ip)[i].pref < prefme)
        prefme = genalloc_s(struct ip_mx, &ip)[i].pref;

  if (relayhost) prefme = 300000;

  for (i = 0;i < genalloc_len(struct ip_mx, &ip) ;++i)
    if (genalloc_s(struct ip_mx, &ip)[i].pref < prefme)
      break;

  if (i >= genalloc_len(struct ip_mx, &ip))
    perm_ambigmx();

  for (i = 0; i < genalloc_len(struct ip_mx, &ip); ++i)
    if (genalloc_s(struct ip_mx, &ip)[i].pref < prefme) {
      if (tcpto(&genalloc_s(struct ip_mx, &ip)[i].ip)) continue;

      smtpfd = socket_tcp4();
      if (smtpfd == -1)
        temp_oserr();

      tain_now(&now);
      tain_addsec(&deadline, &now, timeoutconnect);
      if (socket_deadlineconnstamp46(smtpfd, &genalloc_s(struct ip_mx, &ip)[i].ip,
            (unsigned int) port, &deadline, &now) == 1) {
        tcpto_err(&genalloc_s(struct ip_mx, &ip)[i].ip, 0);
        partner = genalloc_s(struct ip_mx, &ip)[i].ip;
        smtp(); /* does not return */
      }
      tcpto_err(&genalloc_s(struct ip_mx, &ip)[i]. ip, errno == ETIMEDOUT);
      close(smtpfd);
    }

  temp_noconn();
}
