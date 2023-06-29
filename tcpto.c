#include <stdint.h>

#include <unistd.h>

#include <skalibs/djbunix.h>
#include <skalibs/ip46.h>
#include <skalibs/bytestr.h>
#include <skalibs/tai.h>

#include "tcpto.h"
#include "lock.h"

char tcpto_buf[1024];

static int flagwasthere;
static int fdlock;

static int getbuf(void)
{
 int r;
 int fd;

 fdlock = open_write("queue/lock/tcpto");
 if (fdlock == -1) return 0;
 fd = open_read("queue/lock/tcpto");
 if (fd == -1) { close(fdlock); return 0; }
 if (lock_ex(fdlock) == -1) { close(fdlock); close(fd); return 0; }
 r = read(fd,tcpto_buf,sizeof(tcpto_buf));
 close(fd);
 if (r == -1) { close(fdlock); return 0; }
 r >>= 4;
 if (!r) close(fdlock);
 return r;
}

int tcpto(ip46 *ip)
{
  int n;
  int i;
  char *record;

  flagwasthere = 0;

  n = getbuf();
  if (!n) return 0;
  close(fdlock);

  record = tcpto_buf;
  for (i = 0;i < n;++i)
  {
    if (byte_equal(ip->ip,4,record)) /* XXX: wouldn't work with ipv6 addrs*/
    {
      flagwasthere = 1;
      if (record[4] >= 2)
      {
        tai now;
        uint64_t when = (unsigned long) (unsigned char) record[11];
        when = (when << 8) + (unsigned long) (unsigned char) record[10];
        when = (when << 8) + (unsigned long) (unsigned char) record[9];
        when = (when << 8) + (unsigned long) (unsigned char) record[8];

        tai_now(&now);
        if (tai_sec(&now) - when < ((60 + (getpid() & 31)) << 6))
          return 1;
      }
      return 0;
    }
    record += 16;
  }
  return 0;
}

void tcpto_err(ip46 *ip, int flagerr)
{
 int n;
 int i;
 char *record;
 uint64_t when;
 uint64_t lastwhen;
 tai now;

 if (!flagerr)
   if (!flagwasthere)
     return; /* could have been added, but not worth the effort to check */

 n = getbuf();
 if (!n) return;

 record = tcpto_buf;
 for (i = 0;i < n;++i)
  {
   if (byte_equal(ip->ip,4,record)) /* XXX: wouldn't work with ipv6 addrs*/
    {
     if (!flagerr)
       record[4] = 0;
     else
      {
       lastwhen = (unsigned long) (unsigned char) record[11];
       lastwhen = (lastwhen << 8) + (unsigned long) (unsigned char) record[10];
       lastwhen = (lastwhen << 8) + (unsigned long) (unsigned char) record[9];
       lastwhen = (lastwhen << 8) + (unsigned long) (unsigned char) record[8];
       tai_now(&now);
       when = tai_sec(&now);

       if (record[4] && (when < 120 + lastwhen)) { close(fdlock); return; }

       if (++record[4] > 10) record[4] = 10;
       record[8] = when; when >>= 8;
       record[9] = when; when >>= 8;
       record[10] = when; when >>= 8;
       record[11] = when;
      }
     if (lseek(fdlock,i << 4, SEEK_SET) != -1)
       if (write(fdlock,record,16) < 16)
         ; /*XXX*/
     close(fdlock);
     return;
    }
   record += 16;
  }

 if (!flagerr) { close(fdlock); return; }

 record = tcpto_buf;
 for (i = 0;i < n;++i)
  {
   if (!record[4]) break;
   record += 16;
  }

 if (i >= n)
  {
   int firstpos = -1;
   uint64_t firstwhen = 0;
   record = tcpto_buf;
   for (i = 0;i < n;++i)
    {
     when = (unsigned long) (unsigned char) record[11];
     when = (when << 8) + (unsigned long) (unsigned char) record[10];
     when = (when << 8) + (unsigned long) (unsigned char) record[9];
     when = (when << 8) + (unsigned long) (unsigned char) record[8];
     when += (record[4] << 10);
     if ((firstpos < 0) || (when < firstwhen))
      {
       firstpos = i;
       firstwhen = when;
      }
     record += 16;
    }
   i = firstpos;
  }

 if (i >= 0)
  {
   record = tcpto_buf + (i << 4);
   byte_copy(record,4,ip->ip); /* XXX: wouldn't work with ipv6 addrs*/
   tai_now(&now);
   when = tai_sec(&now);
   record[8] = when; when >>= 8;
   record[9] = when; when >>= 8;
   record[10] = when; when >>= 8;
   record[11] = when;
   record[4] = 1;
   if (lseek(fdlock,i << 4, SEEK_SET) != -1)
     if (write(fdlock,record,16) < 16)
       ; /*XXX*/
  }

 close(fdlock);
}
