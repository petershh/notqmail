#ifndef IPALLOC_H
#define IPALLOC_H

#include <skalibs/ip46.h>
/*
#include "ip.h"
*/
struct ip_mx { ip46 ip; int pref; } ;

/*
#include "gen_alloc.h"

GEN_ALLOC_typedef(ipalloc,struct ip_mx,ix,len,a)
extern int ipalloc_readyplus();
extern int ipalloc_append();
*/

#endif
