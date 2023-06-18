#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <errno.h>
#include <unistd.h>

#include <skalibs/bytestr.h>
#include <skalibs/error.h>
#include <skalibs/types.h>
#include <skalibs/buffer.h>

/*
#include "readwrite.h"
#include "substdio.h"
#include "subfd.h"
#include "error.h"
#include "exit.h"
#include "byte.h"
#include "str.h"
#include "case.h"
#include "fmt.h"
*/

#include "auto_users.h"
#include "auto_break.h"
#include "qlx.h"

#define GETPW_USERLEN 32

char *local;
struct passwd *pw;
char *dash;
char *extension;

int userext()
{
  char username[GETPW_USERLEN];
  struct stat st;

  extension = local + str_len(local);
  for (;;) {
    if (extension - local < sizeof(username))
      if (!*extension || (*extension == *auto_break)) {
	byte_copy(username,extension - local,local);
	username[extension - local] = 0;
	case_lowers(username);
	errno = 0;
	pw = getpwnam(username);
	if (errno == ETXTBSY) _exit(QLX_SYS);
	if (pw)
	  if (pw->pw_uid) {
	    if (stat(pw->pw_dir,&st) == 0) {
	      if (st.st_uid == pw->pw_uid) {
		dash = "";
		if (*extension) { ++extension; dash = "-"; }
		return 1;
	      }
	    }
	    else
	      if (error_temp(errno)) _exit(QLX_NFS);
	  }
      }
    if (extension == local) return 0;
    --extension;
  }
}

char num[ULONG_FMT];

int main(int argc, char **argv)
{
  if (argc == 1) return 100;
  local = argv[1];

  if (!userext()) {
    extension = local;
    dash = "-";
    pw = getpwnam(auto_usera);
  }

  if (!pw) return QLX_NOALIAS;

  buffer_puts(buffer_1small,pw->pw_name);
  buffer_put(buffer_1small,"",1);
  buffer_put(buffer_1small,num,ulong_fmt(num,(long) pw->pw_uid));
  buffer_put(buffer_1small,"",1);
  buffer_put(buffer_1small,num,ulong_fmt(num,(long) pw->pw_gid));
  buffer_put(buffer_1small,"",1);
  buffer_puts(buffer_1small,pw->pw_dir);
  buffer_put(buffer_1small,"",1);
  buffer_puts(buffer_1small,dash);
  buffer_put(buffer_1small,"",1);
  buffer_puts(buffer_1small,extension);
  buffer_put(buffer_1small,"",1);
  buffer_flush(buffer_1small);

  return 0;
}
