#include <string.h>
#include <time.h>

#include <skalibs/types.h>
#include <skalibs/tai.h>
#include <skalibs/djbtime.h>

#include "myctime.h"

static char *daytab[7] = {
"Sun","Mon","Tue","Wed","Thu","Fri","Sat"
};
static char *montab[12] = {
"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"
};

static char result[30];

char *myctime(tain const *t)
{
 struct tm dt;
 unsigned int len;
 char *nullbyte;
 localtm_from_tai(&dt, tain_secp(t), 0);
 len = 0;
 nullbyte = strcpy(result + len,daytab[dt.tm_wday]);
 len = nullbyte - result;
 result[len++] = ' ';
 nullbyte = strcpy(result + len,montab[dt.tm_mon]);
 len = nullbyte - result;
 result[len++] = ' ';
 len += uint0_fmt(result + len,dt.tm_mday,2);
 result[len++] = ' ';
 len += uint0_fmt(result + len,dt.tm_hour,2);
 result[len++] = ':';
 len += uint0_fmt(result + len,dt.tm_min,2);
 result[len++] = ':';
 len += uint0_fmt(result + len,dt.tm_sec,2);
 result[len++] = ' ';
 len += uint_fmt(result + len,1900 + dt.tm_year);
 result[len++] = '\n';
 result[len++] = 0;
 return result;
}
