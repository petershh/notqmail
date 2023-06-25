#ifndef FMTQFN_H
#define FMTQFN_H

#include <skalibs/types.h>

extern unsigned int fmtqfn();

#define FMTQFN 40 /* maximum space needed, if len(dirslash) <= 10 */

/* minimum space required */
#define FMTQFN_MIN ULONG_FMT + 1 + ULONG_FMT + 1

#endif
