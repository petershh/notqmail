#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <skalibs/strerr.h>

#include "hier.h"

extern void init_uidgid();

/* TODO this should be a shell script */

void h(char *home, uid_t uid, gid_t gid, int mode)
{
  if (chown(home,uid,gid) == -1)
    strerr_diefu2sys(111,"chown ",home);
  if (chmod(home,mode) == -1)
    strerr_diefu2sys(111,"chmod ",home);
}

void d(char *home, char *subdir, uid_t uid, gid_t gid, int mode)
{
  if (chdir(home) == -1)
    strerr_diefu2sys(111,"switch to ",home);
  if (chown(subdir,uid,gid) == -1)
    strerr_diefu4sys(111,"chown ",home,"/",subdir);
  if (chmod(subdir,mode) == -1)
    strerr_diefu4sys(111,"chmod ",home,"/",subdir);
}

void p(char *home, char *fifo, uid_t uid, gid_t gid, int mode)
{
  if (chdir(home) == -1)
    strerr_diefu2sys(111,"switch to ",home);
  if (chown(fifo,uid,gid) == -1)
    strerr_diefu4sys(111,"chown ",home,"/",fifo);
  if (chmod(fifo,mode) == -1)
    strerr_diefu4sys(111,"chmod ",home,"/",fifo);
}

void c(char *home, char *subdir, char *file, uid_t uid, gid_t gid, int mode)
{
  if (chdir(home) == -1)
    strerr_diefu2sys(111,"switch to ",home);
  if (chdir(subdir) == -1) {
    /* assume cat man pages are simply not installed */
    if (errno == ENOENT && strncmp(subdir, "man/cat", 7) == 0)
      return;
    strerr_diefu4sys(111,"switch to ",home,"/",subdir);
  }
  if (chown(file,uid,gid) == -1) {
    /* assume cat man pages are simply not installed */
    if (errno == ENOENT && strncmp(subdir, "man/cat", 7) == 0)
      return;
    strerr_diefu4sys(111,"chown .../",subdir,"/",file);
  }
  if (chmod(file,mode) == -1)
    strerr_diefu4sys(111,"chmod .../",subdir,"/",file);
}

void z(char *home, char *file, int len, uid_t uid, gid_t gid, int mode)
{
  if (chdir(home) == -1)
    strerr_diefu2sys(111,"switch to ",home);
  if (chown(file,uid,gid) == -1)
    strerr_diefu4sys(111,"chown ",home,"/",file);
  if (chmod(file,mode) == -1)
    strerr_diefu4sys(111,"chmod ",home,"/",file);
}

int main(void)
{
  PROG = "instchown";
  umask(077);
  init_uidgid();
  hier();
  return 0;
}
