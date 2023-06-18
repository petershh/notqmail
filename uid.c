#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>

#include <skalibs/buffer.h>

/*
#include "subfd.h"
#include "substdio.h"
*/

#include "uidgid.h"

uid_t
inituid(char *user)
{
  struct passwd *pw;
  pw = getpwnam(user);
  if (!pw) {
    buffer_puts(buffer_2,"fatal: unable to find user ");
    buffer_puts(buffer_2,user);
    buffer_puts(buffer_2,"\n");
    buffer_flush(buffer_2);
    _exit(111);
  }
  return pw->pw_uid;
}
