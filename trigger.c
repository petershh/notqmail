#include <unistd.h>

#include <skalibs/djbunix.h>
#include <skalibs/iopause.h>

/*
#include "select.h"
#include "open.h"
#include "hasnpbg1.h"
*/

#include "trigger.h"

static int fd = -1;
#ifdef HASNAMEDPIPEBUG1
static int fdw = -1;
#endif

void trigger_set(void)
{
 if (fd != -1)
   close(fd);
#ifdef HASNAMEDPIPEBUG1
 if (fdw != -1)
   close(fdw);
#endif
 fd = open_read("lock/trigger");
#ifdef HASNAMEDPIPEBUG1
 fdw = open_write("lock/trigger");
#endif
}

/* TODO: this stuff doesn't align well with poll or iopause */

void trigger_selprep(iopause_fd *rfd)
{
 if (fd != -1)
  {
    rfd->fd = fd;
    rfd->events = IOPAUSE_READ;
  }
}

int trigger_pulled(iopause_fd *rfd)
{
 return fd != -1 && rfd->revents | IOPAUSE_READ;
}
