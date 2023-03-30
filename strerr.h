#ifndef STRERR_H
#define STRERR_H

#include "noreturn.h"

struct strerr
 {
  struct strerr *who;
  char const *x;
  char const *y;
  char const *z;
 }
;

extern struct strerr strerr_sys;
extern void strerr_sysinit();

extern char *strerr();
extern void qstrerr_warn();
extern void _noreturn_ qstrerr_die();

#define STRERR(r,se,a) \
{ se.who = 0; se.x = a; se.y = 0; se.z = 0; return r; }

#define STRERR_SYS(r,se,a) \
{ se.who = &strerr_sys; se.x = a; se.y = 0; se.z = 0; return r; }
#define STRERR_SYS3(r,se,a,b,c) \
{ se.who = &strerr_sys; se.x = a; se.y = b; se.z = c; return r; }

#define strerr_warn6(x1,x2,x3,x4,x5,x6,se) \
qstrerr_warn((x1),(x2),(x3),(x4),(x5),(x6),(struct strerr *) (se))
#define strerr_warn5(x1,x2,x3,x4,x5,se) \
qstrerr_warn((x1),(x2),(x3),(x4),(x5),NULL,(struct strerr *) (se))
#define strerr_warn4(x1,x2,x3,x4,se) \
qstrerr_warn((x1),(x2),(x3),(x4),NULL,NULL,(struct strerr *) (se))
#define strerr_warn3(x1,x2,x3,se) \
qstrerr_warn((x1),(x2),(x3),NULL,NULL,NULL,(struct strerr *) (se))
#define strerr_warn2(x1,x2,se) \
qstrerr_warn((x1),(x2),NULL,NULL,NULL,NULL,(struct strerr *) (se))
#define strerr_warn1(x1,se) \
qstrerr_warn((x1),NULL,NULL,NULL,NULL,NULL,(struct strerr *) (se))

#define strerr_die6(e,x1,x2,x3,x4,x5,x6,se) \
qstrerr_die((e),(x1),(x2),(x3),(x4),(x5),(x6),(struct strerr *) (se))
#define strerr_die5(e,x1,x2,x3,x4,x5,se) \
qstrerr_die((e),(x1),(x2),(x3),(x4),(x5),NULL,(struct strerr *) (se))
#define strerr_die4(e,x1,x2,x3,x4,se) \
qstrerr_die((e),(x1),(x2),(x3),(x4),NULL,NULL,(struct strerr *) (se))
#define strerr_die3(e,x1,x2,x3,se) \
qstrerr_die((e),(x1),(x2),(x3),NULL,NULL,NULL,(struct strerr *) (se))
#define strerr_die2(e,x1,x2,se) \
qstrerr_die((e),(x1),(x2),NULL,NULL,NULL,NULL,(struct strerr *) (se))
#define strerr_die1(e,x1,se) \
qstrerr_die((e),(x1),NULL,NULL,NULL,NULL,NULL,(struct strerr *) (se))

#define strerr_die6sys(e,x1,x2,x3,x4,x5,x6) \
qstrerr_die((e),(x1),(x2),(x3),(x4),(x5),(x6),&strerr_sys)
#define strerr_die5sys(e,x1,x2,x3,x4,x5) \
qstrerr_die((e),(x1),(x2),(x3),(x4),(x5),NULL,&strerr_sys)
#define strerr_die4sys(e,x1,x2,x3,x4) \
qstrerr_die((e),(x1),(x2),(x3),(x4),NULL,NULL,&strerr_sys)
#define strerr_die3sys(e,x1,x2,x3) \
qstrerr_die((e),(x1),(x2),(x3),NULL,NULL,NULL,&strerr_sys)
#define strerr_die2sys(e,x1,x2) \
qstrerr_die((e),(x1),(x2),NULL,NULL,NULL,NULL,&strerr_sys)
#define strerr_die1sys(e,x1) \
qstrerr_die((e),(x1),NULL,NULL,NULL,NULL,NULL,&strerr_sys)

#define strerr_die6x(e,x1,x2,x3,x4,x5,x6) \
qstrerr_die((e),(x1),(x2),(x3),(x4),(x5),(x6),NULL)
#define strerr_die5x(e,x1,x2,x3,x4,x5) \
qstrerr_die((e),(x1),(x2),(x3),(x4),(x5),NULL,NULL)
#define strerr_die4x(e,x1,x2,x3,x4) \
qstrerr_die((e),(x1),(x2),(x3),(x4),NULL,NULL,NULL)
#define strerr_die3x(e,x1,x2,x3) \
qstrerr_die((e),(x1),(x2),(x3),NULL,NULL,NULL,NULL)
#define strerr_die2x(e,x1,x2) \
qstrerr_die((e),(x1),(x2),NULL,NULL,NULL,NULL,NULL)
#define strerr_die1x(e,x1) \
qstrerr_die((e),(x1),NULL,NULL,NULL,NULL,NULL,NULL)

#endif
