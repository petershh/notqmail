#include <time.h>

#include <skalibs/types.h>

#include "date822fmt.h"
/*
#include "datetime.h"
*/
#include "fmt.h"

static char *montab[12] = {
"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"
};

unsigned int date822fmt(char *s, struct tm *dt)
{
  unsigned int i;
  unsigned int len;
  len = 0;
  i = uint_fmt(s,dt->tm_mday); len += i; if (s) s += i;
  i = fmt_str(s," "); len += i; if (s) s += i;
  i = fmt_str(s,montab[dt->tm_mon]); len += i; if (s) s += i;
  i = fmt_str(s," "); len += i; if (s) s += i;
  i = uint_fmt(s,dt->tm_year + 1900); len += i; if (s) s += i;
  i = fmt_str(s," "); len += i; if (s) s += i;
  i = uint0_fmt(s,dt->tm_hour,2); len += i; if (s) s += i;
  i = fmt_str(s,":"); len += i; if (s) s += i;
  i = uint0_fmt(s,dt->tm_min,2); len += i; if (s) s += i;
  i = fmt_str(s,":"); len += i; if (s) s += i;
  i = uint0_fmt(s,dt->tm_sec,2); len += i; if (s) s += i;
  i = fmt_str(s," -0000\n"); len += i; if (s) s += i;
  return len;
}
