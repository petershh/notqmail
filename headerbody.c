#include <string.h>

#include <skalibs/stralloc.h>
#include <skalibs/buffer.h>

#include "headerbody.h"

#include "hfield.h"
#include "getln.h"

static int getsa(buffer *b, stralloc *sa, int *match)
{
 if (!*match) return 0;
 if (getln(b,sa,match,'\n') == -1) return -1;
 if (*match) return 1;
 if (!sa->len) return 0;
 if (!stralloc_append(sa,'\n')) return -1;
 return 1;
}

static stralloc line = {0};
static stralloc nextline = {0};

int headerbody(buffer *b, void (*dohf)(), void (*hdone)(), void(*dobl)())
{
 int match;
 int flaglineok;
 match = 1;
 flaglineok = 0;
 for (;;)
  {
   switch(getsa(b, &nextline, &match))
    {
     case -1:
       return -1;
     case 0:
       if (flaglineok) dohf(&line);
       hdone();
       /* no message body; could insert blank line here */
       return 0;
    }
   if (flaglineok)
    {
     if ((nextline.s[0] == ' ') || (nextline.s[0] == '\t'))
      {
       if (!stralloc_cat(&line,&nextline)) return -1;
       continue;
      }
     dohf(&line);
    }
   if (nextline.len == 1)
    {
     hdone();
     dobl(&nextline);
     break;
    }
   /* strlen("From ") == 5 */
   if (nextline.len >= 5 && strncmp(nextline.s, "From ", 5) == 0)
    {
     if (!stralloc_copys(&line,"MBOX-Line: ")) return -1;
     if (!stralloc_cat(&line,&nextline)) return -1;
    }
   else
     if (hfield_valid(nextline.s,nextline.len))
      {
       if (!stralloc_copy(&line,&nextline)) return -1;
      }
     else
      {
       hdone();
       if (!stralloc_copys(&line,"\n")) return -1;
       dobl(&line);
       dobl(&nextline);
       break;
      }
   flaglineok = 1;
  }
 for (;;)
   switch(getsa(b,&nextline,&match))
    {
     case -1: return -1;
     case 0: return 0;
     case 1: dobl(&nextline);
    }
}
