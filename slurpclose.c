#include <unistd.h>

#include <skalibs/stralloc.h>
#include <skalibs/allreadwrite.h>

#include "slurpclose.h"

/*
#include "stralloc.h"
#include "readwrite.h"
#include "error.h"
*/

int slurpclose(int fd, stralloc *sa, int bufsize)
{
  int r;
  for (;;) {
    if (!stralloc_readyplus(sa,bufsize)) { close(fd); return -1; }
    r = fd_read(fd,sa->s + sa->len,bufsize);
    if (r == 0 || r == -1) { close(fd); return r; }
    sa->len += r;
  }
}
