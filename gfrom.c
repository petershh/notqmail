#include <skalibs/bytestr.h>
#include "gfrom.h"
/*
#include "str.h"
*/
int gfrom(s,len)
char *s;
int len;
{
 while ((len > 0) && (*s == '>')) { ++s; --len; }
 return (len >= 5) && !str_diffn(s,"From ",5);
}
