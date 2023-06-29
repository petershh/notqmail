#include "hier.h"

#include <sys/stat.h>
#include <stddef.h>

#include <skalibs/djbunix.h>
#include <skalibs/strerr.h>

/*
#include "open.h"
#include "strerr.h"
*/

int main(void)
{
  PROG = "instqueue";
  if (open_read(".") == -1)
    strerr_diefu1sys(111,"open current directory");

  umask(077);
  hier_queue();
  return 0;
}
