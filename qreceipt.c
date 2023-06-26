#include <errno.h>

#include <unistd.h>

#include <skalibs/sig.h>
#include <skalibs/env.h>
#include <skalibs/buffer.h>
#include <skalibs/bytestr.h>
#include <skalibs/genalloc.h>
#include <skalibs/djbunix.h>
#include <skalibs/stralloc.h>

/*
#include "sig.h"
#include "env.h"
#include "substdio.h"
#include "stralloc.h"
#include "subfd.h"
#include "str.h"
#include "error.h"
#include "gen_alloc.h"
#include "gen_allocdefs.h"
#include "exit.h"
#include "open.h"
*/

#include "getln.h"
#include "hfield.h"
#include "token822.h"
#include "headerbody.h"
#include "quote.h"
#include "qmail.h"

void die_noreceipt(void) { _exit(0); }
void die(void) { _exit(100); }
void die_temp(void) { _exit(111); }
void die_nomem(void) {
 buffer_putsflush(buffer_2,"qreceipt: fatal: out of memory\n"); die_temp(); }
void die_fork(void) {
 buffer_putsflush(buffer_2,"qreceipt: fatal: unable to fork\n"); die_temp(); }
void die_qqperm(void) {
 buffer_putsflush(buffer_2,"qreceipt: fatal: permanent qmail-queue error\n"); die(); }
void die_qqtemp(void) {
 buffer_putsflush(buffer_2,"qreceipt: fatal: temporary qmail-queue error\n"); die_temp(); }
void die_usage(void) {
 buffer_putsflush(buffer_2,
 "qreceipt: usage: qreceipt deliveryaddress\n"); die(); }
void die_read(void) {
 if (errno == ENOMEM) die_nomem();
 buffer_putsflush(buffer_2,"qreceipt: fatal: read error\n"); die_temp(); }
void doordie(stralloc *sa, int r) {
 if (r == 1) return; if (r == -1) die_nomem();
 buffer_putsflush(buffer_2,"qreceipt: fatal: unable to parse this: ");
 buffer_putflush(buffer_2,sa->s,sa->len); die(); }

char *target;

int flagreceipt = 0;

char *returnpath;
stralloc messageid = STRALLOC_ZERO;
stralloc sanotice = STRALLOC_ZERO;

int rwnotice(genalloc *addr)
{
  token822_reverse(addr);
  if (token822_unquote(&sanotice,addr) != 1) die_nomem();
  if (sanotice.len == str_len(target))
    if (!str_diffn(sanotice.s,target,sanotice.len))
      flagreceipt = 1;
  token822_reverse(addr); return 1;
}

struct qmail qqt;

stralloc quoted = STRALLOC_ZERO;

void finishheader(void)
{
 char *qqx;

 if (!flagreceipt) die_noreceipt();
 if (str_equal(returnpath,"")) die_noreceipt();
 if (str_equal(returnpath,"#@[]")) die_noreceipt();

 if (!quote2(&quoted,returnpath)) die_nomem();

 if (qmail_open(&qqt) == -1) die_fork();

 qmail_puts(&qqt,"From: DELIVERY NOTICE SYSTEM <");
 qmail_put(&qqt,quoted.s,quoted.len);
 qmail_puts(&qqt,">\n");
 qmail_puts(&qqt,"To: <");
 qmail_put(&qqt,quoted.s,quoted.len);
 qmail_puts(&qqt,">\n");
 qmail_puts(&qqt,"Subject: success notice\n\
\n\
Hi! This is the qreceipt program. Your message was delivered to the\n\
following address: ");
 qmail_puts(&qqt,target);
 qmail_puts(&qqt,". Thanks for asking.\n");
 if (messageid.s) {
   qmail_puts(&qqt,"Your ");
   qmail_put(&qqt,messageid.s,messageid.len);
 }

 qmail_from(&qqt,"");
 qmail_to(&qqt,returnpath);
 qqx = qmail_close(&qqt);

 if (*qqx) {
   if (*qqx == 'D') die_qqperm();
   else die_qqtemp();
 }
}

stralloc hfbuf = STRALLOC_ZERO;
genalloc hfin = GENALLOC_ZERO;
genalloc hfrewrite = GENALLOC_ZERO;
genalloc hfaddr = GENALLOC_ZERO;

void doheaderfield(stralloc *h)
{
 switch(hfield_known(h->s,h->len))
  {
   case H_MESSAGEID:
     if (!stralloc_copy(&messageid,h)) die_nomem();
     break;
   case H_NOTICEREQUESTEDUPONDELIVERYTO:
     doordie(h,token822_parse(&hfin,h,&hfbuf));
     doordie(h,token822_addrlist(&hfrewrite,&hfaddr,&hfin,rwnotice));
     break;
  }
}

void dobody(stralloc *h) { ; }

int main(int argc, char **argv)
{
 sig_ignore(SIGPIPE);
 if (argc == 1) die_usage();
 target = argv[1];
 if (!(returnpath = env_get("SENDER"))) die_usage();
 if (headerbody(buffer_0,doheaderfield,finishheader,dobody) == -1) die_read();
 die_noreceipt();
}
