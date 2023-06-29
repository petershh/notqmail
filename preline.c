#include <unistd.h>
#include <errno.h>

#include <skalibs/buffer.h>
#include <skalibs/strerr.h>
#include <skalibs/djbunix.h>
#include <skalibs/sig.h>
#include <skalibs/sgetopt.h>
#include <skalibs/error.h>
#include <skalibs/env.h>

#include "wait.h"
#include "substdio.h"

void die_usage()
{
  strerr_dieusage(100, "preline cmd [ arg ... ]");
}

int flagufline = 1; char const *ufline;
int flagrpline = 1; char const *rpline;
int flagdtline = 1; char const *dtline;

int main(int argc, char **argv)
{
  int opt;
  int pi[2];
  int pid;
  int wstat;
  subgetopt l = SUBGETOPT_ZERO;
 
  PROG = "preline";
  sig_ignore(SIGPIPE);
 
  if (!(ufline = env_get("UFLINE"))) die_usage();
  if (!(rpline = env_get("RPLINE"))) die_usage();
  if (!(dtline = env_get("DTLINE"))) die_usage();
 
  while ((opt = subgetopt_r(argc, (char const *const *)argv,"frdFRD", &l)) != -1)
    switch(opt) {
      case 'f': flagufline = 0; break;
      case 'r': flagrpline = 0; break;
      case 'd': flagdtline = 0; break;
      case 'F': flagufline = 1; break;
      case 'R': flagrpline = 1; break;
      case 'D': flagdtline = 1; break;
      default: die_usage();
    }
  argc -= l.ind;
  argv += l.ind;
  if (!*argv) die_usage();
 
  if (pipe(pi) == -1)
    strerr_diefu1sys(111, "create pipe");

  pid = fork();
  if (pid == -1)
    strerr_diefu1sys(111, "fork");

  if (pid == 0) {
    close(pi[1]);
    if (fd_move(0,pi[0]) == -1)
      strerr_diefu1sys(111, "set up fds");
    sig_restore(SIGPIPE);
    execvp(*argv, argv);
    strerr_diefu2sys(error_temp(errno) ? 111 : 100, "run ", *argv);
  }
  close(pi[0]);
  if (fd_move(1,pi[1]) == -1)
    strerr_diefu1sys(111, "set up fds");
 
  
  if (flagufline) buffer_puts(buffer_1, ufline);
  if (flagrpline) buffer_puts(buffer_1, rpline);
  if (flagdtline) buffer_puts(buffer_1, dtline);
  if (buffer_copy(buffer_1, buffer_0) != 0)
    strerr_diefu1sys(111, "copy input");
  buffer_flush(buffer_1);
  close(1);
 
  if (waitpid_nointr(pid, &wstat, 0) == -1)
    strerr_dief1sys(111, "wait failed");
  if (wait_crashed(wstat))
    strerr_dief1x(111, "child crashed");
  return wait_exitcode(wstat);
}
