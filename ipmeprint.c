#include <unistd.h>

#include <skalibs/buffer.h>
#include <skalibs/genalloc.h>

#include "ipme.h"

char temp[IP46_FMT];

int main(void)
{
 int j;
 switch(ipme_init())
  {
   case 0: buffer_putsflush(buffer_2, "out of memory\n"); _exit(111);
   case -1: buffer_putsflush(buffer_2, "hard error\n"); _exit(100);
  }
 for (j = 0; j < genalloc_len(struct ip_mx, &ipme); ++j)
  {
   buffer_put(buffer_1, temp,
       ip46_fmt(temp, &genalloc_s(struct ip_mx, &ipme)[j].ip));
   buffer_puts(buffer_1, "\n");
  }
 buffer_flush(buffer_1);
 _exit(0);
}
