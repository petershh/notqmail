#include <string.h>

#include <unistd.h>
/*
#include "stralloc.h"
*/

#include <skalibs/stralloc.h>
#include <skalibs/types.h>

#include "fmt.h"
#include "datetime.h"
#include "newfield.h"
#include "date822fmt.h"

/* "Date: 26 Sep 1995 04:46:53 -0000\n" */
stralloc newfield_date = STRALLOC_ZERO;
/* "Message-ID: <19950926044653.12345.qmail@silverton.berkeley.edu>\n" */
stralloc newfield_msgid = STRALLOC_ZERO;

static unsigned int datefmt(char *s, datetime_sec when)
{
 unsigned int i;
 unsigned int len;
 struct datetime dt;
 datetime_tai(&dt,when);
 len = 0;
 i = fmt_str(s,"Date: "); len += i; if (s) s += i;
 i = date822fmt(s,&dt); len += i; if (s) s += i;
 return len;
}

static unsigned int msgidfmt(char *s, char *idhost, int idhostlen, 
  datetime_sec when)
{
 unsigned int i;
 unsigned int len;
 struct datetime dt;
 datetime_tai(&dt,when);
 len = 0;
 i = fmt_str(s, "Message-ID: <"); len += i; if (s) s += i;
 i = uint_fmt(s,dt.year + 1900); len += i; if (s) s += i;
 i = uint0_fmt(s,dt.mon + 1,2); len += i; if (s) s += i;
 i = uint0_fmt(s,dt.mday,2); len += i; if (s) s += i;
 i = uint0_fmt(s,dt.hour,2); len += i; if (s) s += i;
 i = uint0_fmt(s,dt.min,2); len += i; if (s) s += i;
 i = uint0_fmt(s,dt.sec,2); len += i; if (s) s += i;
 i = fmt_str(s,"."); len += i; if (s) s += i;
 i = uint_fmt(s,getpid()); len += i; if (s) s += i;
 i = fmt_str(s,".qmail@"); len += i; if (s) s += i;
 i = fmt_strn(s,idhost,idhostlen); len += i; if (s) s += i;
 i = fmt_str(s,">\n"); len += i; if (s) s += i;
 return len;
}

int newfield_datemake(datetime_sec when)
{
 if (!stralloc_ready(&newfield_date,datefmt(FMT_LEN,when))) return 0;
 newfield_date.len = datefmt(newfield_date.s,when);
 return 1;
}

int newfield_msgidmake(char *idhost, int idhostlen, datetime_sec when)
{
 if (!stralloc_ready(&newfield_msgid,msgidfmt(FMT_LEN,idhost,idhostlen,when))) return 0;
 newfield_msgid.len = msgidfmt(newfield_msgid.s,idhost,idhostlen,when);
 return 1;
}
