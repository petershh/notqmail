#include <skalibs/stralloc.h>
#include <skalibs/buffer.h>
#include <skalibs/bytestr.h>
#include <skalibs/skamisc.h>

#include "getln.h"

/*
#include "substdio.h"
#include "byte.h"
#include "stralloc.h"
*/

int getln(buffer *b, stralloc *sa, int *match, int sep)
{
  /*
  char *cont;
  unsigned int clen;
  if (getln2(ss,sa,&cont,&clen,sep) == -1) return -1;
  if (!clen) { *match = 0; return 0; }
  if (!stralloc_catb(sa,cont,clen)) return -1;
  *match = 1;
  */

  ssize_t result;

  sa->len = 0;

  for (;;) {
    result = skagetln_nofill(b, sa, sep);
    if (result != 0) {
      if (result > 0)
        *match = 1;
      return result;
    }
    result = buffer_fill(b);
    if (result <= 0) {
      if (result == 0)
        *match = 0;
      return result;
    }
  }

  return 0;
}
