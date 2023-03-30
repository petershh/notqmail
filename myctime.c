#include <string.h>

#include <skalibs/types.h>

#include "myctime.h"

#include "datetime.h"
/*
#include "fmt.h"
*/

static char *daytab[7] = {
"Sun","Mon","Tue","Wed","Thu","Fri","Sat"
};
static char *montab[12] = {
"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"
};

static char result[30];

char *myctime(datetime_sec *t)
{
 struct datetime dt;
 unsigned int len;
 char *nullbyte;
 datetime_tai(&dt,t);
 len = 0;
 nullbyte = strcpy(result + len,daytab[dt.wday]);
 len = nullbyte - result;
 result[len++] = ' ';
 nullbyte = strcpy(result + len,montab[dt.mon]);
 len = nullbyte - result;
 result[len++] = ' ';
 len += uint0_fmt(result + len,dt.mday,2);
 result[len++] = ' ';
 len += uint0_fmt(result + len,dt.hour,2);
 result[len++] = ':';
 len += uint0_fmt(result + len,dt.min,2);
 result[len++] = ':';
 len += uint0_fmt(result + len,dt.sec,2);
 result[len++] = ' ';
 len += uint_fmt(result + len,1900 + dt.year);
 result[len++] = '\n';
 result[len++] = 0;
 return result;
}
