#include <errno.h>
#include <unistd.h>

#include <skalibs/env.h>
#include <skalibs/sig.h>
#include <skalibs/buffer.h>
#include <skalibs/stralloc.h>
#include <skalibs/genalloc.h>
#include <skalibs/sgetopt.h>
#include <skalibs/bytestr.h>
#include <skalibs/types.h>
#include <skalibs/tai.h>

#include "getln.h"
#include "hfield.h"
#include "token822.h"
#include "control.h"
#include "qmail.h"
#include "quote.h"
#include "headerbody.h"
#include "auto_qmail.h"
#include "newfield.h"
#include "constmap.h"
#include "noreturn.h"


#define LINELEN 80

tain starttime;

char const *qmopts;
int flagdeletesender = 0;
int flagdeletefrom = 0;
int flagdeletemessid = 0;
int flagnamecomment = 0;
int flaghackmess = 0;
int flaghackrecip = 0;
char const *mailhost;
char const *mailuser;
int mailusertokentype;
char const *mailrhost;
char const *mailruser;

stralloc control_idhost = STRALLOC_ZERO;
stralloc control_defaultdomain = STRALLOC_ZERO;
stralloc control_defaulthost = STRALLOC_ZERO;
stralloc control_plusdomain = STRALLOC_ZERO;

stralloc sender = STRALLOC_ZERO;
stralloc envsbuf = STRALLOC_ZERO;
genalloc envs = GENALLOC_ZERO;
int flagrh;

int flagqueue;
struct qmail qqt;

void put(char *s,int len)
{ if (flagqueue) qmail_put(&qqt,s,len); else buffer_put(buffer_1,s,len); }
void puts(s) char *s; { put(s,str_len(s)); }

void _noreturn_ perm() { _exit(100); }
void _noreturn_ temp() { _exit(111); }
void _noreturn_ die_nomem() {
 buffer_putsflush(buffer_2,"qmail-inject: fatal: out of memory\n"); temp(); }
void _noreturn_ die_invalid(sa) stralloc *sa; {
 buffer_putsflush(buffer_2,"qmail-inject: fatal: invalid header field: ");
 buffer_putflush(buffer_2,sa->s,sa->len); perm(); }
void _noreturn_ die_qqt() {
 buffer_putsflush(buffer_2,"qmail-inject: fatal: unable to run qmail-queue\n"); temp(); }
void _noreturn_ die_chdir() {
 buffer_putsflush(buffer_2,"qmail-inject: fatal: internal bug\n"); temp(); }
void _noreturn_ die_read() {
 if (errno == ENOMEM) die_nomem();
 buffer_putsflush(buffer_2,"qmail-inject: fatal: read error\n"); temp(); }
void doordie(stralloc *sa, int r) {
 if (r == 1) return; if (r == -1) die_nomem();
 buffer_putsflush(buffer_2,"qmail-inject: fatal: unable to parse this line:\n");
 buffer_putflush(buffer_2,sa->s,sa->len); perm(); }
/* call doordie, but if q is set ignore parse errors (i.e. r == 0) */
static int doordie_rh(stralloc *sa, int r, int q)
{
  if (q && r == 0)
    return 0;
  doordie(sa, r);
  return 1;
}

/*
GEN_ALLOC_typedef(saa,stralloc,sa,len,a)
GEN_ALLOC_readyplus(saa,stralloc,sa,len,a,10,saa_readyplus)
*/

static stralloc sauninit = STRALLOC_ZERO;

genalloc savedh = GENALLOC_ZERO;
genalloc hrlist = GENALLOC_ZERO;
genalloc tocclist = GENALLOC_ZERO;
genalloc hrrlist = GENALLOC_ZERO;
genalloc reciplist = GENALLOC_ZERO;
int flagresent;

void _noreturn_ exitnicely()
{
 char *qqx;

 if (!flagqueue) buffer_flush(buffer_1);

 if (flagqueue)
  {
   int i;

   if (!stralloc_0(&sender)) die_nomem();
   qmail_from(&qqt,sender.s);

   for (i = 0;i < reciplist.len;++i)
    {
     if (!stralloc_0(&genalloc_s(stralloc,&reciplist)[i])) die_nomem();
     qmail_to(&qqt,genalloc_s(stralloc,&reciplist)[i].s);
    }
   if (flagrh) {
     if (flagresent)
       for (i = 0;i < genalloc_len(stralloc,&hrrlist);++i)
	{
         if (!stralloc_0(&genalloc_s(stralloc,&hrrlist)[i])) die_nomem();
	 qmail_to(&qqt,genalloc_s(stralloc,&hrrlist)[i].s);
	}
     else
       for (i = 0;i < genalloc_len(stralloc,&hrlist);++i)
	{
         if (!stralloc_0(&genalloc_s(stralloc,&hrrlist)[i])) die_nomem();
	 qmail_to(&qqt,genalloc_s(stralloc,&hrlist)[i].s);
	}
   }

   qqx = qmail_close(&qqt);
   if (*qqx) {
     if (*qqx == 'D') {
       buffer_puts(buffer_2,"qmail-inject: fatal: ");
       buffer_puts(buffer_2,qqx + 1);
       buffer_puts(buffer_2,"\n");
       buffer_flush(buffer_2);
       perm();
     }
     else {
       buffer_puts(buffer_2,"qmail-inject: fatal: ");
       buffer_puts(buffer_2,qqx + 1);
       buffer_puts(buffer_2,"\n");
       buffer_flush(buffer_2);
       temp();
     }
   }
  }

 _exit(0);
}

void savedh_append(stralloc *h)
{
 if (!genalloc_readyplus(stralloc,&savedh,1)) die_nomem();
 genalloc_s(stralloc,&savedh)[genalloc_len(stralloc,&savedh)] = sauninit;
 if (!stralloc_copy(genalloc_s(stralloc,&savedh)
             + genalloc_len(stralloc,&savedh),h)) die_nomem();
 ++savedh.len;
}

void savedh_print()
{
 int i;

 for (i = 0;i < genalloc_len(stralloc,&savedh);++i)
   put(genalloc_s(stralloc,&savedh)[i].s,genalloc_s(stralloc,&savedh)[i].len);
}

stralloc defaultdomainbuf = STRALLOC_ZERO;
genalloc defaultdomain = GENALLOC_ZERO;
stralloc defaulthostbuf = STRALLOC_ZERO;
genalloc defaulthost = GENALLOC_ZERO;
stralloc plusdomainbuf = STRALLOC_ZERO;
genalloc plusdomain = GENALLOC_ZERO;

void rwroute(genalloc *addr)
{
 struct token822 *addr_arr = token822_s(addr);
 size_t addr_len = token822_len(addr);
 if (addr_arr[addr_len - 1].type == TOKEN822_AT)
   while (addr_len)
     addr_len--;
     token822_setlen(addr, addr_len);
     if (addr_arr[addr_len].type == TOKEN822_COLON)
       return;
}

void rwextraat(genalloc *addr)
{
 int i;
 struct token822 *addr_arr = token822_s(addr);
 size_t addr_len = token822_len(addr);
 if (addr_arr[0].type == TOKEN822_AT)
  {
   addr_len--;
   token822_setlen(addr, addr_len);
   for (i = 0;i < addr_len;++i)
     addr_arr[i] = addr_arr[i + 1];
  }
}

void rwextradot(genalloc *addr)
{
 int i;
 struct token822 *addr_arr = token822_s(addr);
 size_t addr_len = token822_len(addr);
 if (addr_arr[0].type == TOKEN822_DOT)
  {
   addr_len--;
   token822_setlen(addr, addr_len);
   for (i = 0;i < addr->len;++i)
     addr_arr[i] = addr_arr[i + 1];
  }
}

void rwnoat(genalloc *addr)
{
 int i;
 int shift;
 struct token822 *addr_arr = token822_s(addr);
 size_t addr_len = token822_len(addr);

 for (i = 0;i < addr_len;++i)
   if (addr_arr[i].type == TOKEN822_AT)
     return;
 shift = token822_len(&defaulthost);
 if (!token822_readyplus(addr,shift)) die_nomem();
 for (i = addr_len - 1;i >= 0;--i)
   addr_arr[i + shift] = addr_arr[i];
 addr_len += shift;
 token822_setlen(addr, addr_len);
 for (i = 0;i < shift;++i)
   addr_arr[i] = token822_s(&defaulthost)[shift - 1 - i];
}

void rwnodot(genalloc *addr)
{
 int i;
 int shift;
 struct token822 *addr_arr = token822_s(addr);
 size_t addr_len = token822_len(addr);
 for (i = 0;i < addr_len;++i)
  {
   if (addr_arr[i].type == TOKEN822_DOT)
     return;
   if (addr_arr[i].type == TOKEN822_AT)
     break;
  }
 for (i = 0;i < addr_len;++i)
  {
   if (addr_arr[i].type == TOKEN822_LITERAL)
     return;
   if (addr_arr[i].type == TOKEN822_AT)
     break;
  }
 shift = token822_len(&defaultdomain);
 if (!token822_readyplus(addr,shift)) die_nomem();
 for (i = addr_len - 1;i >= 0;--i)
   addr_arr[i + shift] = addr_arr[i];
 addr_len += shift;
 token822_setlen(addr, addr_len);
 for (i = 0;i < shift;++i)
   addr_arr[i] = token822_s(&defaultdomain)[shift - 1 - i];
}

void rwplus(genalloc *addr)
{
 int i;
 int shift;
 size_t addr_len = token822_len(addr);
 struct token822 *addr_arr = token822_s(addr);

 if (addr_arr[0].type != TOKEN822_ATOM) return;
 if (!addr_arr[0].slen) return;
 if (addr_arr[0].s[addr_arr[0].slen - 1] != '+') return;

 --addr_arr[0].slen; /* remove + */

 shift = token822_len(&plusdomain);
 if (!token822_readyplus(addr,shift)) die_nomem();
 for (i = addr_len - 1;i >= 0;--i)
   addr_arr[i + shift] = addr_arr[i];
 addr_len += shift;
 token822_setlen(addr, addr_len);
 for (i = 0;i < shift;++i)
   addr_arr[i] = token822_s(&plusdomain)[shift - 1 - i];
}

void rwgeneric(genalloc *addr)
{
 size_t addr_len = token822_len(addr);
 struct token822 *addr_arr = token822_s(addr);
 if (!addr_len) return; /* don't rewrite <> */
 if (addr_len >= 2)
   if (addr_arr[1].type == TOKEN822_AT)
     if (addr_arr[0].type == TOKEN822_LITERAL)
       if (!addr_arr[0].slen) /* don't rewrite <foo@[]> */
	 return;
 rwroute(addr);
 if (!token822_len(addr)) return; /* <@foo:> -> <> */
 rwextradot(addr);
 if (!token822_len(addr)) return; /* <.> -> <> */
 rwextraat(addr);
 if (!token822_len(addr)) return; /* <@> -> <> */
 rwnoat(addr);
 rwplus(addr);
 rwnodot(addr);
}

int setreturn(genalloc *addr)
{
 if (!sender.s)
  {
   token822_reverse(addr);
   if (token822_unquote(&sender,addr) != 1) die_nomem();
   if (flaghackrecip)
     if (!stralloc_cats(&sender,"-@[]")) die_nomem();
   token822_reverse(addr);
  }
 return 1;
}

int rwreturn(genalloc *addr)
{
 rwgeneric(addr);
 setreturn(addr);
 return 1;
}

int rwsender(genalloc *addr)
{
 rwgeneric(addr);
 return 1;
}

void rwappend(genalloc *addr, genalloc *xl)
{
 token822_reverse(addr);
 if (!genalloc_readyplus(stralloc,xl,1)) die_nomem();
 genalloc_s(stralloc,xl)[genalloc_len(stralloc,xl)] = sauninit;
 if (token822_unquote(genalloc_s(stralloc,xl) + genalloc_len(stralloc,xl),
             addr) != 1) die_nomem();
 ++xl->len;
 token822_reverse(addr);
}

int rwhrr(genalloc *addr)
{ rwgeneric(addr); rwappend(addr,&hrrlist); return 1; }
int rwhr(genalloc *addr)
{ rwgeneric(addr); rwappend(addr,&hrlist); return 1; }
int rwtocc(genalloc *addr)
{ rwgeneric(addr); rwappend(addr,&hrlist); rwappend(addr,&tocclist); return 1; }

int htypeseen[H_NUM];
stralloc hfbuf = STRALLOC_ZERO;
genalloc hfin = GENALLOC_ZERO;
genalloc hfrewrite = GENALLOC_ZERO;
genalloc hfaddr = GENALLOC_ZERO;

void doheaderfield(stralloc *h)
{
  int htype;
  int (*rw)() = 0;
  int rwmayfail = 0;
 
  htype = hfield_known(h->s,h->len);
  if (flagdeletefrom) if (htype == H_FROM) return;
  if (flagdeletemessid) if (htype == H_MESSAGEID) return;
  if (flagdeletesender) if (htype == H_RETURNPATH) return;
 
  if (htype)
    htypeseen[htype] = 1;
  else
    if (!hfield_valid(h->s,h->len))
      die_invalid(h);
 
  switch(htype) {
    case H_TO: case H_CC:
      rw = rwtocc;
      rwmayfail = 1;
      break;
    case H_BCC: case H_APPARENTLYTO:
      rw = rwhr;
      rwmayfail = 1;
      break;
    case H_R_TO: case H_R_CC: case H_R_BCC:
      rw = rwhrr;
      rwmayfail = 1;
      break;
    case H_RETURNPATH:
      rw = rwreturn; break;
    case H_SENDER: case H_FROM: case H_REPLYTO:
    case H_RETURNRECEIPTTO: case H_ERRORSTO:
    case H_R_SENDER: case H_R_FROM: case H_R_REPLYTO:
      rw = rwsender; break;
  }

  if (rw) {
    if (doordie_rh(h,token822_parse(&hfin,h,&hfbuf),rwmayfail) &&
        doordie_rh(h,token822_addrlist(&hfrewrite,&hfaddr,&hfin,rw),rwmayfail)) {
      if (token822_unparse(h,&hfrewrite,LINELEN) != 1)
        die_nomem();
    }
  }
 
  if (htype == H_BCC) return;
  if (htype == H_R_BCC) return;
  if (htype == H_RETURNPATH) return;
  if (htype == H_CONTENTLENGTH) return; /* some things are just too stupid */
  savedh_append(h);
}

void dobody(stralloc *h)
{
 put(h->s,h->len);
}

stralloc torecip = STRALLOC_ZERO;
genalloc tr = GENALLOC_ZERO;

void dorecip(char *s)
{
 if (!quote2(&torecip,s)) die_nomem();
 switch(token822_parse(&tr,&torecip,&hfbuf))
  {
   case -1: die_nomem();
   case 0:
     buffer_puts(buffer_2,"qmail-inject: fatal: unable to parse address: ");
     buffer_puts(buffer_2,s);
     buffer_putsflush(buffer_2,"\n");
     perm();
  }
 token822_reverse(&tr);
 rwgeneric(&tr);
 rwappend(&tr,&reciplist);
}

stralloc defaultfrom = STRALLOC_ZERO;
genalloc df = GENALLOC_ZERO;

void defaultfrommake(void)
{
 char const *fullname;
 struct token822 *df_arr;
 fullname = env_get("QMAILNAME");
 df_arr = token822_s(&df);
 if (!fullname) fullname = env_get("MAILNAME");
 if (!fullname) fullname = env_get("NAME");
 if (!token822_ready(&df,20)) die_nomem();
 token822_setlen(&df,0);
 df_arr[0].type = TOKEN822_ATOM;
 df_arr[0].s = "From";
 df_arr[0].slen = 4;
 token822_setlen(&df, 1);
 df_arr[1].type = TOKEN822_COLON;
 token822_setlen(&df, 2);
 if (fullname && !flagnamecomment)
  {
   df_arr[2].type = TOKEN822_QUOTE;
   df_arr[2].s = fullname;
   df_arr[2].slen = str_len(fullname);
   token822_setlen(&df, 3);
   df_arr[3].type = TOKEN822_LEFT;
   token822_setlen(&df, 4);
  }
 df_arr[token822_len(&df)].type = mailusertokentype;
 df_arr[token822_len(&df)].s = mailuser;
 df_arr[token822_len(&df)].slen = str_len(mailuser);
 token822_deltalen(&df, 1);
 if (mailhost)
  {
   df_arr[token822_len(&df)].type = TOKEN822_AT;
   token822_deltalen(&df, 1);
   df_arr[token822_len(&df)].type = TOKEN822_ATOM;
   df_arr[token822_len(&df)].s = mailhost;
   df_arr[token822_len(&df)].slen = str_len(mailhost);
   token822_deltalen(&df, 1);
  }
 if (fullname && !flagnamecomment)
  {
   df_arr[token822_len(&df)].type = TOKEN822_RIGHT;
   token822_deltalen(&df, 1);
  }
 if (fullname && flagnamecomment)
  {
   df_arr[token822_len(&df)].type = TOKEN822_COMMENT;
   df_arr[token822_len(&df)].s = fullname;
   df_arr[token822_len(&df)].slen = str_len(fullname);
   token822_deltalen(&df, 1);
  }
 if (token822_unparse(&defaultfrom,&df,LINELEN) != 1) die_nomem();
 doordie(&defaultfrom,token822_parse(&df,&defaultfrom,&hfbuf));
 doordie(&defaultfrom,token822_addrlist(&hfrewrite,&hfaddr,&df,rwsender));
 if (token822_unparse(&defaultfrom,&hfrewrite,LINELEN) != 1) die_nomem();
}

stralloc defaultreturnpath = STRALLOC_ZERO;
genalloc drp = GENALLOC_ZERO;
stralloc hackedruser = STRALLOC_ZERO;
char strnum[TAIN_FMT];

void dodefaultreturnpath(void)
{
 struct token822 *drp_arr = token822_s(&drp);
 if (!stralloc_copys(&hackedruser,mailruser)) die_nomem();
 if (flaghackmess)
  {
   if (!stralloc_cats(&hackedruser,"-")) die_nomem();
   if (!stralloc_catb(&hackedruser,strnum,tain_fmt(strnum, &starttime))) die_nomem();
   if (!stralloc_cats(&hackedruser,".")) die_nomem();
   if (!stralloc_catb(&hackedruser,strnum,ulong_fmt(strnum,(unsigned long) getpid()))) die_nomem();
  }
 if (flaghackrecip)
   if (!stralloc_cats(&hackedruser,"-")) die_nomem();
 if (!token822_ready(&drp,10)) die_nomem();
 token822_setlen(&drp, 0);
 drp_arr[token822_len(&drp)].type = TOKEN822_ATOM;
 drp_arr[token822_len(&drp)].s = "Return-Path";
 drp_arr[token822_len(&drp)].slen = 11;
 token822_deltalen(&drp, 1);
 drp_arr[token822_len(&drp)].type = TOKEN822_COLON;
 token822_deltalen(&drp, 1);
 drp_arr[token822_len(&drp)].type = TOKEN822_QUOTE;
 drp_arr[token822_len(&drp)].s = hackedruser.s;
 drp_arr[token822_len(&drp)].slen = hackedruser.len;
 token822_deltalen(&drp, 1);
 if (mailrhost)
  {
   drp_arr[token822_len(&drp)].type = TOKEN822_AT;
   token822_deltalen(&drp, 1);
   drp_arr[token822_len(&drp)].type = TOKEN822_ATOM;
   drp_arr[token822_len(&drp)].s = mailrhost;
   drp_arr[token822_len(&drp)].slen = str_len(mailrhost);
   token822_deltalen(&drp, 1);
  }
 if (token822_unparse(&defaultreturnpath,&drp,LINELEN) != 1) die_nomem();
 doordie(&defaultreturnpath,token822_parse(&drp,&defaultreturnpath,&hfbuf));
 doordie(&defaultreturnpath
   ,token822_addrlist(&hfrewrite,&hfaddr,&drp,rwreturn));
 if (token822_unparse(&defaultreturnpath,&hfrewrite,LINELEN) != 1) die_nomem();
}

int flagmft = 0;
stralloc mft = STRALLOC_ZERO;
struct constmap mapmft;

void mft_init(void)
{
  char const *x;
  int r;

  x = env_get("QMAILMFTFILE");
  if (!x) return;

  r = control_readfile(&mft,x,0);
  if (r == -1) die_read(); /*XXX*/
  if (!r) return;

  if (!constmap_init(&mapmft,mft.s,mft.len,0)) die_nomem();
  flagmft = 1;
}

void finishmft(void)
{
  int i;
  static stralloc sa = STRALLOC_ZERO;
  static stralloc sa2 = STRALLOC_ZERO;

  if (!flagmft) return;
  if (htypeseen[H_MAILFOLLOWUPTO]) return;

  for (i = 0;i < genalloc_len(stralloc,&tocclist);++i)
    if (constmap(&mapmft,genalloc_s(stralloc, &tocclist)[i].s,
                genalloc_s(stralloc, &tocclist)[i].len))
      break;

  if (i == genalloc_len(stralloc, &tocclist)) return;

  puts("Mail-Followup-To: ");
  i = genalloc_len(stralloc, &tocclist);
  while (i--) {
    if (!stralloc_copy(&sa,genalloc_s(stralloc, &tocclist) + i)) die_nomem();
    if (!stralloc_0(&sa)) die_nomem();
    if (!quote2(&sa2,sa.s)) die_nomem();
    put(sa2.s,sa2.len);
    if (i) puts(",\n  ");
  }
  puts("\n");
}

void finishheader(void)
{
 flagresent =
   htypeseen[H_R_SENDER] || htypeseen[H_R_FROM] || htypeseen[H_R_REPLYTO]
   || htypeseen[H_R_TO] || htypeseen[H_R_CC] || htypeseen[H_R_BCC]
   || htypeseen[H_R_DATE] || htypeseen[H_R_MESSAGEID];

 if (!sender.s)
   dodefaultreturnpath();

 if (!flagqueue)
  {
   static stralloc sa = {0};
   static stralloc sa2 = {0};

   if (!stralloc_copy(&sa,&sender)) die_nomem();
   if (!stralloc_0(&sa)) die_nomem();
   if (!quote2(&sa2,sa.s)) die_nomem();

   puts("Return-Path: <");
   put(sa2.s,sa2.len);
   puts(">\n");
  }

 /* could check at this point whether there are any recipients */
 if (flagqueue)
   if (qmail_open(&qqt) == -1) die_qqt();

 if (flagresent)
  {
   if (!htypeseen[H_R_DATE])
    {
     if (!newfield_datemake(tain_secp(&starttime))) die_nomem();
     puts("Resent-");
     put(newfield_date.s,newfield_date.len);
    }
   if (!htypeseen[H_R_MESSAGEID])
    {
     if (!newfield_msgidmake(control_idhost.s,control_idhost.len,tain_secp(&starttime))) die_nomem();
     puts("Resent-");
     put(newfield_msgid.s,newfield_msgid.len);
    }
   if (!htypeseen[H_R_FROM])
    {
     defaultfrommake();
     puts("Resent-");
     put(defaultfrom.s,defaultfrom.len);
    }
   if (!htypeseen[H_R_TO] && !htypeseen[H_R_CC])
     puts("Resent-Cc: recipient list not shown: ;\n");
  }
 else
  {
   if (!htypeseen[H_DATE])
    {
     if (!newfield_datemake(tain_secp(&starttime))) die_nomem();
     put(newfield_date.s,newfield_date.len);
    }
   if (!htypeseen[H_MESSAGEID])
    {
     if (!newfield_msgidmake(control_idhost.s,control_idhost.len,tain_secp(&starttime))) die_nomem();
     put(newfield_msgid.s,newfield_msgid.len);
    }
   if (!htypeseen[H_FROM])
    {
     defaultfrommake();
     put(defaultfrom.s,defaultfrom.len);
    }
   if (!htypeseen[H_TO] && !htypeseen[H_CC])
     puts("Cc: recipient list not shown: ;\n");
   finishmft();
  }

 savedh_print();
}

void getcontrols(void)
{
 static stralloc sa = STRALLOC_ZERO;
 char const *x;

 mft_init();

 if (chdir(auto_qmail) == -1) die_chdir();
 if (control_init() == -1) die_read();

 if (control_rldef(&control_defaultdomain,"control/defaultdomain",1,"defaultdomain") != 1)
   die_read();
 x = env_get("QMAILDEFAULTDOMAIN");
 if (x) if (!stralloc_copys(&control_defaultdomain,x)) die_nomem();
 if (!stralloc_copys(&sa,".")) die_nomem();
 if (!stralloc_cat(&sa,&control_defaultdomain)) die_nomem();
 doordie(&sa,token822_parse(&defaultdomain,&sa,&defaultdomainbuf));

 if (control_rldef(&control_defaulthost,"control/defaulthost",1,"defaulthost") != 1)
   die_read();
 x = env_get("QMAILDEFAULTHOST");
 if (x) if (!stralloc_copys(&control_defaulthost,x)) die_nomem();
 if (!stralloc_copys(&sa,"@")) die_nomem();
 if (!stralloc_cat(&sa,&control_defaulthost)) die_nomem();
 doordie(&sa,token822_parse(&defaulthost,&sa,&defaulthostbuf));

 if (control_rldef(&control_plusdomain,"control/plusdomain",1,"plusdomain") != 1)
   die_read();
 x = env_get("QMAILPLUSDOMAIN");
 if (x) if (!stralloc_copys(&control_plusdomain,x)) die_nomem();
 if (!stralloc_copys(&sa,".")) die_nomem();
 if (!stralloc_cat(&sa,&control_plusdomain)) die_nomem();
 doordie(&sa,token822_parse(&plusdomain,&sa,&plusdomainbuf));

 if (control_rldef(&control_idhost,"control/idhost",1,"idhost") != 1)
   die_read();
 x = env_get("QMAILIDHOST");
 if (x) if (!stralloc_copys(&control_idhost,x)) die_nomem();
}

#define RECIP_DEFAULT 1
#define RECIP_ARGS 2
#define RECIP_HEADER 3
#define RECIP_AH 4

int main(int argc, char **argv)
{
 int i;
 int opt;
 int recipstrategy;
 subgetopt l = SUBGETOPT_ZERO;

 sig_ignore(SIGPIPE);

 tain_now(&starttime);

 qmopts = env_get("QMAILINJECT");
 if (qmopts)
   while (*qmopts)
     switch(*qmopts++)
      {
       case 'c': flagnamecomment = 1; break;
       case 's': flagdeletesender = 1; break;
       case 'f': flagdeletefrom = 1; break;
       case 'i': flagdeletemessid = 1; break;
       case 'r': flaghackrecip = 1; break;
       case 'm': flaghackmess = 1; break;
      }

 mailhost = env_get("QMAILHOST");
 if (!mailhost) mailhost = env_get("MAILHOST");
 mailrhost = env_get("QMAILSHOST");
 if (!mailrhost) mailrhost = mailhost;

 mailuser = env_get("QMAILUSER");
 if (!mailuser) mailuser = env_get("MAILUSER");
 if (!mailuser) mailuser = env_get("USER");
 if (!mailuser) mailuser = env_get("LOGNAME");
 if (!mailuser) mailuser = "anonymous";
 mailusertokentype = TOKEN822_ATOM;
 if (quote_need(mailuser,str_len(mailuser))) mailusertokentype = TOKEN822_QUOTE;
 mailruser = env_get("QMAILSUSER");
 if (!mailruser) mailruser = mailuser;

 for (i = 0;i < H_NUM;++i) htypeseen[i] = 0;

 recipstrategy = RECIP_DEFAULT;
 flagqueue = 1;

 getcontrols();

 if (!genalloc_readyplus(stralloc,&hrlist,1)) die_nomem();
 if (!genalloc_readyplus(stralloc,&tocclist,1)) die_nomem();
 if (!genalloc_readyplus(stralloc,&hrrlist,1)) die_nomem();
 if (!genalloc_readyplus(stralloc,&reciplist,1)) die_nomem();

 while ((opt = subgetopt_r(argc,(char const *const *)argv,"aAhHnNf:",&l)) != -1)
   switch(opt)
    {
     case 'a': recipstrategy = RECIP_ARGS; break;
     case 'A': recipstrategy = RECIP_DEFAULT; break;
     case 'h': recipstrategy = RECIP_HEADER; break;
     case 'H': recipstrategy = RECIP_AH; break;
     case 'n': flagqueue = 0; break;
     case 'N': flagqueue = 1; break;
     case 'f':
       if (!quote2(&sender,l.arg)) die_nomem();
       doordie(&sender,token822_parse(&envs,&sender,&envsbuf));
       token822_reverse(&envs);
       rwgeneric(&envs);
       token822_reverse(&envs);
       if (token822_unquote(&sender,&envs) != 1) die_nomem();
       break;
     case '?':
     default:
       perm();
    }
 argc -= l.ind;
 argv += l.ind;

 if (recipstrategy == RECIP_DEFAULT)
   recipstrategy = (*argv ? RECIP_ARGS : RECIP_HEADER);

 if (recipstrategy != RECIP_HEADER)
   while (*argv)
     dorecip(*argv++);

 flagrh = (recipstrategy != RECIP_ARGS);

 if (headerbody(buffer_0,doheaderfield,finishheader,dobody) == -1)
   die_read();
 exitnicely();
}
