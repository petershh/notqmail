#include <unistd.h>

#include <skalibs/djbunix.h>
#include <skalibs/buffer.h>

#include "tcpto.h"

char tcpto_cleanbuf[1024];

void tcpto_clean(void) /* running from queue/mess */
{
 int fd;
 int i;
 buffer b;

 fd = open_write("../lock/tcpto");
 if (fd == -1) return;
 buffer_init(&b,buffer_write,fd,tcpto_cleanbuf,sizeof(tcpto_cleanbuf));
 for (i = 0;i < sizeof(tcpto_cleanbuf);++i) buffer_put(&b,"",1);
 buffer_flush(&b); /* if it fails, bummer */
 close(fd);
}
