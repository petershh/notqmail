#include <unistd.h>

#include <skalibs/djbunix.h>

#include "triggerpull.h"

void triggerpull(void)
{
 int fd;

 fd = open_write("lock/trigger");
 if (fd >= 0)
  {
   ndelay_on(fd);
   write(fd,"",1); /* if it fails, bummer */
   close(fd);
  }
}
