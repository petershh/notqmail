#include <skalibs/genalloc.h>

#include "prioq.h"
/*
#include "gen_allocdefs.h"
*/

/*
GEN_ALLOC_readyplus(prioq,struct prioq_elt,p,len,a,100,prioq_readyplus)
*/

int prioq_insert(genalloc *pq, struct prioq_elt *pe)
{
 int i;
 int j;
 struct prioq_elt *pq_p = genalloc_s(struct prioq_elt, pq);
 if (!genalloc_readyplus(struct prioq_elt,pq,1)) return 0;
 j = pq->len++;
 while (j)
  {
   i = (j - 1)/2;
   if (pq_p[i].dt <= pe->dt) break;
   pq_p[j] = pq_p[i];
   j = i;
  }
 pq_p[j] = *pe;
 return 1;
}

int prioq_min(genalloc *pq, struct prioq_elt *pe)
{
 struct prioq_elt *pq_p = genalloc_s(struct prioq_elt, pq);
 if (!pq_p) return 0;
 if (!pq->len) return 0;
 *pe = pq_p[0];
 return 1;
}

void prioq_delmin(genalloc *pq)
{
 int i;
 int j;
 int n;
 struct prioq_elt *pq_p = genalloc_s(struct prioq_elt, pq);
 if (!pq_p) return;
 n = genalloc_len(struct prioq_elt, pq);
 if (!n) return;
 i = 0;
 --n;
 for (;;)
  {
   j = i + i + 2;
   if (j > n) break;
   if (pq_p[j - 1].dt <= pq_p[j].dt) --j;
   if (pq_p[n].dt <= pq_p[j].dt) break;
   pq_p[i] = pq_p[j];
   i = j;
  }
 pq_p[i] = pq_p[n];
 genalloc_setlen(struct prioq_elt, pq, n);
}
