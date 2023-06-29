#include <sys/stat.h>
#include <unistd.h>

#include <skalibs/strerr.h>

#define FATAL "maildirmake: fatal: "

int main(int argc, char **argv)
{
  umask(077);
  PROG = "maildirmake";
  if (!argv[1])
    strerr_dieusage(100, "maildirmake name");
  if (mkdir(argv[1],0700) == -1)
    strerr_diefu2sys(111, "mkdir ", argv[1]);
  if (chdir(argv[1]) == -1)
    strerr_diefu2sys(111, "chdir to ", argv[1]);
  if (mkdir("tmp",0700) == -1)
    strerr_diefu3sys(111, "mkdir ", argv[1], "/tmp");
  if (mkdir("new",0700) == -1)
    strerr_diefu3sys(111, "mkdir ", argv[1], "/new");
  if (mkdir("cur",0700) == -1)
    strerr_diefu3sys(111, "mkdir ", argv[1], "/cur");
  return 0;
}
