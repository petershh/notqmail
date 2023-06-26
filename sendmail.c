#include <unistd.h>

#include <skalibs/sgetopt.h>
#include <skalibs/buffer.h>
#include <skalibs/alloc.h>
#include <skalibs/env.h>
#include <skalibs/bytestr.h>
#include <skalibs/stralloc.h>
#include <skalibs/exec.h>

/*
#include "sgetopt.h"
#include "substdio.h"
#include "subfd.h"
#include "alloc.h"
#include "exit.h"
#include "env.h"
#include "str.h"
*/

#include "auto_qmail.h"
#include "noreturn.h"

void _noreturn_ nomem(void)
{
  buffer_putsflush(buffer_2,"sendmail: fatal: out of memory\n");
  _exit(111);
}

void _noreturn_ die_usage(void)
{
  buffer_putsflush(buffer_2,"sendmail: usage: sendmail [ -t ] [ -fsender ] [ -Fname ] [ -bp ] [ -bs ] [ arg ... ]\n");
  _exit(100);
}

char *smtpdarg[] = { "bin/qmail-smtpd", 0 };
void _noreturn_ smtpd(stralloc *env_modifs)
{
  if (!env_get("PROTO")) {
    if (!env_addmodif(env_modifs, "RELAYCLIENT", "")) nomem();
    if (!env_addmodif(env_modifs, "DATABYTES", "0")) nomem();
    if (!env_addmodif(env_modifs, "PROTO", "TCP")) nomem();
    if (!env_addmodif(env_modifs, "TCPLOCALIP", "127.0.0.1")) nomem();
    if (!env_addmodif(env_modifs, "TCPLOCALHOST", "localhost")) nomem();
    if (!env_addmodif(env_modifs, "TCPREMOTEIP", "127.0.0.1")) nomem();
    if (!env_addmodif(env_modifs, "TCPREMOTEHOST", "localhost")) nomem();
    if (!env_addmodif(env_modifs, "TCPREMOTEINFO", "sendmail-bs")) nomem();
  }
  mexec_m((char const *const *)smtpdarg, env_modifs->s, env_modifs->len);
  buffer_putsflush(buffer_2,"sendmail: fatal: unable to run qmail-smtpd\n");
  _exit(111);
}

char *qreadarg[] = { "bin/qmail-qread", 0 };
void _noreturn_ mailq(stralloc *env_modifs)
{
  mexec_m((char const *const *)qreadarg, env_modifs->s, env_modifs->len);
  buffer_putsflush(buffer_2,"sendmail: fatal: unable to run qmail-qread\n");
  _exit(111);
}

void do_sender(const char *s, stralloc *env_modifs)
{
  char *x;
  unsigned int n;
  unsigned int a;
  unsigned int i;
  
  env_addmodif(env_modifs, "QMAILNAME", NULL);
  env_addmodif(env_modifs, "MAILNAME", NULL);
  env_addmodif(env_modifs, "NAME", NULL);
  env_addmodif(env_modifs, "QMAILHOST", NULL);
  env_addmodif(env_modifs, "MAILHOST", NULL);

  n = str_len(s);
  a = str_rchr(s, '@');
  if (a == n)
  {
    env_addmodif(env_modifs, "QMAILUSER", s);
    return;
  }
  env_addmodif(env_modifs, "QMAILHOST", s + a + 1);

  x = (char *) alloc((a + 1) * sizeof(char));
  if (!x) nomem();
  for (i = 0; i < a; i++)
    x[i] = s[i];
  x[i] = 0;
  env_addmodif(env_modifs, "QMAILUSER", x);
  alloc_free(x);
}

int flagh;
char const *sender;

int main(int argc, char **argv)
{
  int opt;
  char **qiargv;
  char **arg;
  int i;
  char const *progname = argv[0];
  subgetopt l = SUBGETOPT_ZERO;
  stralloc modifs = STRALLOC_ZERO;
 
  if (chdir(auto_qmail) == -1) {
    buffer_putsflush(buffer_2,"sendmail: fatal: unable to switch to qmail home directory\n");
    return 111;
  }

  flagh = 0;
  sender = 0;
  while ((opt = subgetopt_r(argc, (char const *const *)argv, 
                  "vimte:f:p:o:B:F:EJxb:", &l)) != -1)
    switch(opt) {
      case 'B': break;
      case 't': flagh = 1; break;
      case 'f': sender = l.arg; break;
      case 'F': if (!env_addmodif(&modifs, "MAILNAME", l.arg)) nomem(); break;
      case 'p': break; /* could generate a Received line from optarg */
      case 'v': break;
      case 'i': break; /* what an absurd concept */
      case 'x': break; /* SVR4 stupidity */
      case 'm': break; /* twisted-paper-path blindness, incompetent design */
      case 'e': break; /* qmail has only one error mode */
      case 'o':
        switch(l.arg[0]) {
	  case 'd': break; /* qmail has only one delivery mode */
	  case 'e': break; /* see 'e' above */
	  case 'i': break; /* see 'i' above */
	  case 'm': break; /* see 'm' above */
	}
        break;
      case 'E': case 'J': /* Sony NEWS-OS */
        while (argv[l.ind][l.pos]) ++l.pos; /* skip optional argument */
        break;
      case 'b':
	switch(optarg[0]) {
	  case 'm': break;
	  case 'p': mailq(&modifs);
	  case 's': smtpd(&modifs);
	  default: die_usage();
	}
	break;
      default:
	die_usage();
    }
  argc -= l.ind;
  argv += l.ind;
 
  if (str_equal(progname,"mailq"))
    mailq(&modifs);

  if (str_equal(progname,"newaliases")) {
    buffer_putsflush(buffer_2,"sendmail: fatal: please use fastforward/newaliases instead\n");
    return 100;
  }

  qiargv = (char **) alloc((argc + 10) * sizeof(char *));
  if (!qiargv) nomem();
 
  arg = qiargv;
  *arg++ = "bin/qmail-inject";
  *arg++ = (flagh ? "-H" : "-a");
  if (sender) {
    *arg++ = "-f";
    *arg++ = sender;
    do_sender(sender, &modifs);
  }
  *arg++ = "--";
  for (i = 0;i < argc;++i) *arg++ = argv[i];
  *arg = 0;
 
  mexec_m((char const *const *)qiargv, modifs.s, modifs.len);
  buffer_putsflush(buffer_2,"sendmail: fatal: unable to run qmail-inject\n");
  return 111;
}
