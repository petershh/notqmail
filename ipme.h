#ifndef IPME_H
#define IPME_H

#include <skalibs/ip46.h>
#include <skalibs/genalloc.h>

#include "ipalloc.h"

extern genalloc ipme;

extern int ipme_init();
extern int ipme_is();

#endif
