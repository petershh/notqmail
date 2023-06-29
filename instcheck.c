#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <skalibs/strerr.h>
/*
#include "strerr.h"
#include "error.h"
#include "readwrite.h"
*/
#include "hier.h"

extern void init_uidgid();

void perm(char *prefix1, char *prefix2, char *prefix3, char *file, int type,
          uid_t uid, gid_t gid, int mode)
{
  struct stat st;

  if (stat(file,&st) == -1) {
    if (errno == ENOENT) {
      /* assume cat man pages are simply not installed */
      if (strncmp(prefix2, "man/cat", 7) != 0 && strncmp(file, "man/cat", 7) != 0)
        strerr_warnw5x(prefix1,prefix2,prefix3,file," does not exist");
    } else
      strerr_warnwu2sys("stat .../",file);
    return;
  }

  if ((uid != -1) && (st.st_uid != uid))
    strerr_warnw5x(prefix1,prefix2,prefix3,file," has wrong owner");
  if ((gid != -1) && (st.st_gid != gid))
    strerr_warnw5x(prefix1,prefix2,prefix3,file," has wrong group");
  if ((st.st_mode & 07777) != mode)
    strerr_warnw5x(prefix1,prefix2,prefix3,file," has wrong permissions");
  if ((st.st_mode & S_IFMT) != type)
    strerr_warnw5x(prefix1,prefix2,prefix3,file," has wrong type");
}

void h(char *home, uid_t uid, gid_t gid, int mode)
{
  perm("","","",home,S_IFDIR,uid,gid,mode);
}

void d(char *home, char *subdir, uid_t uid, gid_t gid, int mode)
{
  if (chdir(home) == -1)
    strerr_diefu2sys(111,"switch to ",home);
  perm("",home,"/",subdir,S_IFDIR,uid,gid,mode);
}

void p(char *home, char *fifo, uid_t uid, gid_t gid, int mode)
{
  if (chdir(home) == -1)
    strerr_diefu2sys(111,"switch to ",home);
  perm("",home,"/",fifo,S_IFIFO,uid,gid,mode);
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
  perm(".../",subdir,"/",file,S_IFREG,uid,gid,mode);
}

void z(char *home, char *file, int len, uid_t uid, gid_t gid, int mode)
{
  if (chdir(home) == -1)
    strerr_diefu2sys(111,"switch to ",home);
  perm("",home,"/",file,S_IFREG,uid,gid,mode);
}

int main(void)
{
  PROG = "instcheck";
  init_uidgid();
  hier();
  return 0;
}
