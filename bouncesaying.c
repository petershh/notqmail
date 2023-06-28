#if 0
#include "fork.h"
#include "strerr.h"
#include "error.h"
#include "wait.h"
#include "sig.h"
#include "exit.h"
#include "error_temp.h"

#define FATAL "bouncesaying: fatal: "
#endif
#include <errno.h>

#include <unistd.h>

#include <skalibs/strerr.h>
#include <skalibs/djbunix.h>
#include <skalibs/error.h>
#include <skalibs/buffer.h>

#include "wait.h"

#define USAGE "bouncesaying error [ program [ arg ... ] ]"

int main(int argc, char **argv)
{
    pid_t pid;
    int wstat;
    PROG = "bouncesaying";
    if (argc == 1)
        strerr_dieusage(100, USAGE);

    if (argv[2]) {
        pid = fork();
        if (pid == -1)
            strerr_diefu1sys(111, "fork");
        if (pid == 0) {
            execvp(argv[2], argv + 2);
            if (error_temp(errno))
                _exit(111);
            _exit(100);
        }
        if (wait_pid(pid, &wstat) == -1)
            strerr_dief1x(111, "wait failed");
        if (wait_crashed(wstat))
            strerr_dief1x(111, "child crashed");
        switch(wait_exitcode(wstat)) {
            case 0:
                break;
            case 111:
                strerr_dief1x(111, "temporary child error");
                /* not reached, so no fallthrough */
            default:
                _exit(0);
        }
    }

    buffer_putsflush(buffer_2, argv[1]);
    return 100;
}
