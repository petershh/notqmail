#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include <skalibs/buffer.h>
#include <skalibs/strerr.h>
#include <skalibs/env.h>
#include <skalibs/stralloc.h>
#include <skalibs/bytestr.h>
#include <skalibs/djbunix.h>

#include "substdio.h"

int fdsourcedir = -1;

static void die_nomem(void)
{
  strerr_dief1x(111, "out of memory");
}

static void ddhome(stralloc *dd, char *home)
{
  const char *denv = env_get("DESTDIR");
  if (denv)
    if (!stralloc_copys(dd,denv)) die_nomem();

  if (!stralloc_catb(dd,home,str_len(home))) die_nomem();
  if (!stralloc_0(dd)) die_nomem();
}

static int mkdir_p(char *home, int mode)
{
  stralloc parent = STRALLOC_ZERO;
  unsigned int sl;
  int r = mkdir(home,mode);
  if (!r || errno != ENOENT)
    return r;

  /* try parent first */
  sl = str_rchr(home, '/');
  if (!stralloc_copyb(&parent,home,sl)) die_nomem();
  if (!stralloc_0(&parent)) die_nomem();
  r = mkdir_p(parent.s,mode);
  free(parent.s);
  if (r && errno != EEXIST)
    return r;

  return mkdir(home,mode);
}

void h(char *home, uid_t uid, gid_t gid, int mode)
{
  stralloc dh = STRALLOC_ZERO;
  ddhome(&dh, home);
  home=dh.s;
  if (mkdir_p(home,mode) == -1)
    if (errno != EEXIST)
      strerr_diefu2sys(111, "mkdir ", home);
  if (chmod(home,mode) == -1)
    strerr_diefu2sys(111,"chmod ",home);
  free(dh.s);
}

void d(char *home, char *subdir, uid_t uid, gid_t gid, int mode)
{
  stralloc dh = STRALLOC_ZERO;
  ddhome(&dh, home);
  home=dh.s;
  if (chdir(home) == -1)
    strerr_diefu2sys(111,"switch to ",home);
  if (mkdir(subdir,0700) == -1)
    if (errno != EEXIST)
      strerr_diefu4sys(111,"mkdir ",home,"/",subdir);
  if (chmod(subdir,mode) == -1)
    strerr_diefu4sys(111,"chmod ",home,"/",subdir);
  free(dh.s);
}

void p(char *home, char *fifo, uid_t uid, gid_t gid, int mode)
{
  stralloc dh = STRALLOC_ZERO;
  ddhome(&dh, home);
  home=dh.s;
  if (chdir(home) == -1)
    strerr_diefu2sys(111,"switch to ",home);
  if (mkfifo(fifo,0700) == -1)
    if (errno != EEXIST)
      strerr_diefu4sys(111,"mkfifo ",home,"/",fifo);
  if (chmod(fifo,mode) == -1)
    strerr_diefu4sys(111,"chmod ",home,"/",fifo);
  free(dh.s);
}

char inbuf[BUFFER_INSIZE];
char outbuf[BUFFER_OUTSIZE];
buffer bin;
buffer bout;

void c(char *home, char *subdir, char *file, uid_t uid, gid_t gid, int mode)
{
  int fdin;
  int fdout;
  stralloc dh = STRALLOC_ZERO;

  if (fchdir(fdsourcedir) == -1)
    strerr_diefu1sys(111,"switch back to source directory");

  fdin = open_read(file);
  if (fdin == -1) {
    /* silently ignore missing catman pages */
    if (errno == ENOENT && strncmp(subdir, "man/cat", 7) == 0)
      return;
    strerr_diefu2sys(111,"read ",file);
  }

  /* if the user decided to build only dummy catman pages then don't install */
  if (strncmp(subdir, "man/cat", 7) == 0) {
    struct stat st;
    if (fstat(fdin, &st) != 0)
      strerr_diefu2sys(111,"stat ",file);
    if (st.st_size == 0) {
      close(fdin);
      return;
    }
  }

  ddhome(&dh, home);
  home=dh.s;

  buffer_init(&bin,buffer_read,fdin,inbuf,sizeof(inbuf));

  if (chdir(home) == -1)
    strerr_diefu2sys(111,"switch to ",home);
  if (chdir(subdir) == -1)
    strerr_diefu4sys(111,"switch to ",home,"/",subdir);

  fdout = open_trunc(file);
  if (fdout == -1)
    strerr_diefu4sys(111,"write .../",subdir,"/",file);
  buffer_init(&bout,buffer_write,fdout,outbuf,sizeof(outbuf));

  switch(buffer_copy(&bout,&bin)) {
    case -2:
      strerr_diefu2sys(111,"read ",file);
    case -3:
      strerr_diefu4sys(111,"write .../",subdir,"/",file);
  }

  close(fdin);
  if (buffer_flush(&bout) == -1)
    strerr_diefu4sys(111,"write .../",subdir,"/",file);
  if (fsync(fdout) == -1)
    strerr_diefu4sys(111,"write .../",subdir,"/",file);
  if (close(fdout) == -1) /* NFS silliness */
    strerr_diefu4sys(111,"write .../",subdir,"/",file);

  if (chmod(file,mode) == -1)
    strerr_diefu4sys(111,"chmod .../",subdir,"/",file);
  free(dh.s);
}

void z(char *home, char *file, int len, uid_t uid, gid_t gid, int mode)
{
  int fdout;
  stralloc dh = STRALLOC_ZERO;

  ddhome(&dh, home);
  home=dh.s;
  if (chdir(home) == -1)
    strerr_diefu2sys(111,"switch to ",home);

  fdout = open_trunc(file);
  if (fdout == -1)
    strerr_diefu4sys(111,"write ",home,"/",file);
  buffer_init(&bout,buffer_write,fdout,outbuf,sizeof(outbuf));

  while (len-- > 0)
    if (buffer_put(&bout,"",1) == -1)
      strerr_diefu4sys(111,"write ",home,"/",file);

  if (buffer_flush(&bout) == -1)
    strerr_diefu4sys(111,"write ",home,"/",file);
  if (fsync(fdout) == -1)
    strerr_diefu4sys(111,"write ",home,"/",file);
  if (close(fdout) == -1) /* NFS silliness */
    strerr_diefu4sys(111,"write ",home,"/",file);

  if (chmod(file,mode) == -1)
    strerr_diefu4sys(111,"chmod ",home,"/",file);
  free(dh.s);
}

/* these are ignored, but hier() passes them to h() and friends */
uid_t auto_uida = -1;
uid_t auto_uido = -1;
uid_t auto_uidq = -1;
uid_t auto_uidr = -1;
uid_t auto_uids = -1;

gid_t auto_gidq = -1;
