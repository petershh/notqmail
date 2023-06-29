#include "hier.h"

#include <sys/stat.h>
#include <stddef.h>

#include <skalibs/djbunix.h>
#include <skalibs/strerr.h>

/*
#include "open.h"
#include "strerr.h"
*/

#define FATAL "instpackage: fatal: "

extern int fdsourcedir;

int main(void)
{
  PROG = "instpackage";
  fdsourcedir = open_read(".");
  if (fdsourcedir == -1)
    strerr_diefu1sys(111, "open current directory");

  umask(077);
  hier();
  return 0;
}
