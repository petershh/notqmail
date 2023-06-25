#ifndef PRIOQ_H
#define PRIOQ_H

#include <skalibs/tai.h>

/*
#include "datetime.h"
#include "gen_alloc.h"
*/

struct prioq_elt { tai dt; unsigned long id; } ;

/* GEN_ALLOC_typedef(prioq,struct prioq_elt,p,len,a) */

extern int prioq_insert();
extern int prioq_min();
extern void prioq_delmin();

#endif
