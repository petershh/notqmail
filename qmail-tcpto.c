/* XXX: this program knows quite a bit about tcpto's internals */
#include <errno.h>
#include <stdint.h>
#include <string.h>

#include <unistd.h>

#include <skalibs/buffer.h>
#include <skalibs/bytestr.h>
#include <skalibs/ip46.h>
#include <skalibs/djbunix.h>
#include <skalibs/tai.h>
#include <skalibs/types.h>

/*
#include "substdio.h"
#include "subfd.h"
#include "byte.h"
#include "fmt.h"
#include "ip.h"
#include "open.h"
#include "error.h"
#include "exit.h"
#include "datetime.h"
#include "now.h"
*/

#include "auto_qmail.h"
#include "lock.h"

void die(int n) { buffer_flush(buffer_1); _exit(n); }

void warn(char *s)
{
 char *x;
 x = strerror(errno);
 buffer_puts(buffer_1,s);
 buffer_puts(buffer_1,": ");
 buffer_puts(buffer_1,x);
 buffer_puts(buffer_1,"\n");
}

void die_chdir(void) { warn("fatal: unable to chdir"); die(111); }
void die_open(void) { warn("fatal: unable to open tcpto"); die(111); }
void die_lock(void) { warn("fatal: unable to lock tcpto"); die(111); }
void die_read(void) { warn("fatal: unable to read tcpto"); die(111); }

char tcpto_buf[1024];

char tmp[ULONG_FMT + IP46_FMT];

int main(void)
{
 int fdlock;
 int fd;
 int r;
 int i;
 char *record;
 ip46 ip;
 uint64_t when;
 tain start;

 if (chdir(auto_qmail) == -1) die_chdir();
 if (chdir("queue/lock") == -1) die_chdir();

 fdlock = open_write("tcpto");
 if (fdlock == -1) die_open();
 fd = open_read("tcpto");
 if (fd == -1) die_open();
 if (lock_ex(fdlock) == -1) die_lock();
 r = read(fd,tcpto_buf,sizeof(tcpto_buf));
 close(fd);
 close(fdlock);

 if (r == -1) die_read();
 r >>= 4;

 tain_now(&start);

 record = tcpto_buf;
 for (i = 0;i < r;++i)
  {
   if (record[4] >= 1)
    {
     byte_copy(&ip,4,record);
     when = (unsigned long) (unsigned char) record[11];
     when = (when << 8) + (unsigned long) (unsigned char) record[10];
     when = (when << 8) + (unsigned long) (unsigned char) record[9];
     when = (when << 8) + (unsigned long) (unsigned char) record[8];

     buffer_put(buffer_1,tmp,ip46_fmt(tmp,&ip));
     buffer_puts(buffer_1," timed out ");
     buffer_put(buffer_1,tmp,ulong_fmt(tmp,(unsigned long) (tai_sec(tain_secp(&start)) - when)));
     buffer_puts(buffer_1," seconds ago; # recent timeouts: ");
     buffer_put(buffer_1,tmp,ulong_fmt(tmp,(unsigned long) (unsigned char) record[4]));
     buffer_puts(buffer_1,"\n");
    }
   record += 16;
  }

 die(0);
}
