#include <sys/types.h>
#include <grp.h>
#include <unistd.h>

#include <skalibs/buffer.h>

#include "uidgid.h"

gid_t
initgid(char *group)
{
  struct group *gr;
  gr = getgrnam(group);
  if (!gr) {
    buffer_puts(buffer_2,"fatal: unable to find group ");
    buffer_puts(buffer_2,group);
    buffer_puts(buffer_2,"\n");
    buffer_flush(buffer_2);
    _exit(111);
  }
  return gr->gr_gid;
}
