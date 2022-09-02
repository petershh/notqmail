#include <errno.h>

#include <unistd.h>

#include <skalibs/strerr2.h>
#include <skalibs/djbunix.h>
#include <skalibs/error.h>

#include "wait.h"
/*
#include "fork.h"
#include "strerr.h"
#include "error.h"
#include "exit.h"
#define FATAL "except: fatal: "
*/

#define USAGE "except program [ arg ... ]"

int main(int argc, char **argv)
{
  int pid;
  int wstat;
  PROG = "except";

  if (argc == 1)
    strerr_dieusage(100, USAGE);

  pid = fork();
  if (pid == -1)
    strerr_diefu1sys(111, "fork");
  if (pid == 0) {
    execvp(argv[1],argv + 1);
    if (error_temp(errno)) _exit(111);
    _exit(100);
  }

  if (wait_pid(pid, &wstat) == -1)
    strerr_dief1x(111, "wait failed");
  if (wait_crashed(wstat))
    strerr_dief1x(111, "child crashed");
  switch(wait_exitcode(wstat)) {
    case 0: _exit(100);
    case 111: strerr_dief1x(111, "temporary child error");
    default: _exit(0);
  }
}
