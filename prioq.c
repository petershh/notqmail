#include <skalibs/genalloc.h>
#include <skalibs/tai.h>

#include "prioq.h"

int prioq_insert(genalloc *pq, struct prioq_elt *pe)
{
 int i;
 int j;
 struct prioq_elt *pq_arr = genalloc_s(struct prioq_elt, pq);
 if (!genalloc_readyplus(struct prioq_elt,pq,1)) return 0;
 j = genalloc_len(struct prioq_elt, pq);
 genalloc_setlen(struct prioq_elt, pq, genalloc_len(struct prioq_elt, pq) + 1);
 while (j) {
   i = (j - 1)/2;
   if (!tain_less(&pe->dt, &pq_arr[i].dt)) break;
   pq_arr[j] = pq_arr[i];
   j = i;
 }
 pq_arr[j] = *pe;
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
 struct prioq_elt *pq_arr = genalloc_s(struct prioq_elt, pq);
 if (!pq_arr) return;
 n = genalloc_len(struct prioq_elt, pq);
 if (!n) return;
 i = 0;
 --n;
 for (;;)
  {
   j = i + i + 2;
   if (j > n) break;
   if (!tain_less(&pq_arr[j].dt, &pq_arr[j - 1].dt)) --j;
   if (!tain_less(&pq_arr[j].dt, &pq_arr[n].dt)) break;
   pq_arr[i] = pq_arr[j];
   i = j;
  }
 pq_arr[i] = pq_arr[n];
 genalloc_setlen(struct prioq_elt, pq, n);
}
