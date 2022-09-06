#include <unistd.h>

#include <skalibs/buffer.h>

/*
#include "substdio.h"
#include "subfd.h"
#include "readwrite.h"
*/


char host[256];

int main(void)
{
 host[0] = 0; /* sigh */
 gethostname(host,sizeof(host));
 host[sizeof(host) - 1] = 0;
 buffer_puts(buffer_1small, host);
 buffer_puts(buffer_1small, "\n");
 buffer_flush(buffer_1small);
 return 0;
}
