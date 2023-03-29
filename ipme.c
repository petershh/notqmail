#include <string.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#ifndef SIOCGIFCONF /* whatever works */
#include <sys/sockio.h>
#endif
#include <unistd.h>
#include "hassalen.h"
/*
#include "byte.h"
#include "ip.h"
#include "stralloc.h"
*/

#include <skalibs/ip46.h>
#include <skalibs/genalloc.h>
#include <skalibs/bytestr.h>

#include "ipalloc.h"
#include "ipme.h"

static int ipmeok = 0;
genalloc ipme = GENALLOC_ZERO;

int ipme_is(ip46 *ip)
{
  int i;
  if (ipme_init() != 1) return -1;
  for (i = 0; i < genalloc_len(struct ip_mx, &ipme); ++i)
    if (!byte_diff(&genalloc_s(struct ip_mx, &ipme)[i].ip, 4, ip))
      return 1;
  return 0;
}

static stralloc buf = {0};

int ipme_init(void)
{
  struct ifconf ifc;
  char *x;
  struct ifreq *ifr;
  struct sockaddr_in *sin;
  int len;
  int s;
  struct ip_mx ix;
 
  if (ipmeok) return 1;
  if (!genalloc_readyplus(struct ip_mx, &ipme, 0)) return 0;
  ipme.len = 0;
  ix.pref = 0;
 
  /* 0.0.0.0 is a special address which always refers to 
   * "this host, this network", according to RFC 1122, Sec. 3.2.1.3a.
  */
  memcpy(ix.ip.ip, IP6_ANY, 16);
  ix.ip.is6 = 0;
  if (!genalloc_append(struct ip_mx, &ipme, &ix)) { return 0; }
  if ((s = socket(AF_INET,SOCK_STREAM,0)) == -1) return -1;

  ifc.ifc_buf = 0;
  ifc.ifc_len = 0;

  /* first pass: just ask what the correct length for all addresses is */
  len = 0;
  if (ioctl(s,SIOCGIFCONF,&ifc) >= 0 && ifc.ifc_len > 0) { /* > is for System V */
    if (!stralloc_ready(&buf,ifc.ifc_len)) { close(s); return 0; }
    ifc.ifc_buf = buf.s;
    if (ioctl(s,SIOCGIFCONF,&ifc) >= 0)
      buf.len = ifc.ifc_len;
  }

  /* check if we have complete length, otherwise try so sort that out */
  if (buf.len == 0) {
    len = 256;
    for (;;) {
      if (!stralloc_ready(&buf,len)) { close(s); return 0; }
      buf.len = 0;
      ifc.ifc_buf = buf.s;
      ifc.ifc_len = len;
      if (ioctl(s,SIOCGIFCONF,&ifc) >= 0) /* > is for System V */
        if (ifc.ifc_len + sizeof(struct ifreq) + 64 < len) { /* what a stupid interface */
          buf.len = ifc.ifc_len;
          break;
        }
      if (len > 200000) { close(s); return -1; }
      len += 100 + (len >> 2);
    }
  }
  x = buf.s;
  while (x < buf.s + buf.len) {
    ifr = (struct ifreq *) x;
#ifdef HASSALEN
    len = sizeof(ifr->ifr_name) + ifr->ifr_addr.sa_len;
    if (len < sizeof(*ifr))
      len = sizeof(*ifr);
#else
    len = sizeof(*ifr);
#endif
    if (ifr->ifr_addr.sa_family == AF_INET) {
      sin = (struct sockaddr_in *) &ifr->ifr_addr;
      byte_copy(&ix.ip.ip,4,&sin->sin_addr);
      if (ioctl(s,SIOCGIFFLAGS,x) == 0)
        if (ifr->ifr_flags & IFF_UP)
          if (!genalloc_append(struct ip_mx, &ipme, &ix)) {
              close(s); return 0;
          }
    }
    x += len;
  }
  close(s);
  ipmeok = 1;
  return 1;
}
