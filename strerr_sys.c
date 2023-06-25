#include <errno.h>
#include <string.h>

#include "strerr.h"

/* explicit initialization due to linker error on Mac OS X. dorks. */
struct strerr strerr_sys = {0,0,0,0};

void strerr_sysinit()
{
  strerr_sys.who = 0;
  strerr_sys.x = strerror(errno);
  strerr_sys.y = "";
  strerr_sys.z = "";
}
