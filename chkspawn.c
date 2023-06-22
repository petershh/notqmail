#if 0
#include "substdio.h"
#include "subfd.h"
#include "fmt.h"
#include "select.h"
#endif

#include <sys/select.h>

#include <skalibs/types.h>
#include <skalibs/buffer.h>

#include "auto_spawn.h"

char num[ULONG_FMT];
fd_set fds;

int main(void)
{
    unsigned long hiddenlimit;
    unsigned long maxnumd;

    hiddenlimit = sizeof(fds) * 8;
    maxnumd = (hiddenlimit - 5) / 2;

    if (auto_spawn < 1) {
        buffer_putsflush(buffer_2, "Oops. You have set conf-spawn lower than 1.\n");
        return 1;
    }

    if (auto_spawn > 255) {
        buffer_putsflush(buffer_2, "Oops. You have set conf-spawn higher than 255.\n");
        return 1;
    }

    if (auto_spawn > maxnumd) {
        buffer_puts(buffer_2, "Oops. Your system's FD_SET() has a hidden limit of ");
        buffer_put(buffer_2, num, ulong_fmt(num,hiddenlimit));
        buffer_puts(buffer_2, " descriptors.\n\
                This means that the qmail daemons could crash if you set the run-time\n\
                concurrency higher than ");
        buffer_put(buffer_2,num, ulong_fmt(num,maxnumd));
        buffer_puts(buffer_2, ". So I'm going to insist that the concurrency\n\
                limit in conf-spawn be at most ");
        buffer_put(buffer_2, num, ulong_fmt(num,maxnumd));
        buffer_puts(buffer_2, ". Right now it's ");
        buffer_put(buffer_2, num, ulong_fmt(num,(unsigned long) auto_spawn));
        buffer_puts(buffer_2, ".\n");
        buffer_flush(buffer_2);
        return 1;
    }
    return 0;
}
