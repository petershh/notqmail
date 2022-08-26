#include <sys/uio.h>

#include <skalibs/buffer.h>
#include "substdio.h"

int buffer_copy(buffer *bout, buffer *bin)
{
  struct iovec v[2];

  for (;;) {
    ssize_t n; 
    if (buffer_isempty(bin)) {
      n = buffer_fill(ssin);
      if (n < 0) return -2;
      if (!n) return 0;
    }
    buffer_wpeek(bin, iov);
    n = buffer_putvflush(bout, v, 2);
    if (n == -1)
      return -3;
    buffer_rseek(bin, n);
  }
}
