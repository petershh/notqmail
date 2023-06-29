#include <skalibs/stralloc.h>
#include <skalibs/buffer.h>
#include <skalibs/bytestr.h>
#include <skalibs/skamisc.h>

#include "getln.h"

int getln(buffer *b, stralloc *sa, int *match, int sep)
{

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
