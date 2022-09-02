#include <skalibs/types.h>

#include "date822fmt.h"

#include "datetime.h"
#include "fmt.h"

static char *montab[12] = {
"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"
};

unsigned int date822fmt(s,dt)
char *s;
struct datetime *dt;
{
  unsigned int i;
  unsigned int len;
  len = 0;
  i = uint_fmt(s,dt->mday); len += i; if (s) s += i;
  i = fmt_str(s," "); len += i; if (s) s += i;
  i = fmt_str(s,montab[dt->mon]); len += i; if (s) s += i;
  i = fmt_str(s," "); len += i; if (s) s += i;
  i = uint_fmt(s,dt->year + 1900); len += i; if (s) s += i;
  i = fmt_str(s," "); len += i; if (s) s += i;
  i = uint0_fmt(s,dt->hour,2); len += i; if (s) s += i;
  i = fmt_str(s,":"); len += i; if (s) s += i;
  i = uint0_fmt(s,dt->min,2); len += i; if (s) s += i;
  i = fmt_str(s,":"); len += i; if (s) s += i;
  i = uint0_fmt(s,dt->sec,2); len += i; if (s) s += i;
  i = fmt_str(s," -0000\n"); len += i; if (s) s += i;
  return len;
}
