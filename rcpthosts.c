#include <errno.h>

#include <skalibs/bytestr.h>
#include <skalibs/cdb.h>
#include <skalibs/stralloc.h>

#include "rcpthosts.h"

/*
#include "case.h"
#include "cdb.h"
#include "byte.h"
#include "open.h"
#include "error.h"
#include "stralloc.h"
*/

#include "control.h"
#include "constmap.h"

static int flagrh = 0;
static stralloc rh = STRALLOC_ZERO;
static struct constmap maprh;
static cdb db = CDB_ZERO;
static int db_is_loaded = 0;

int rcpthosts_init()
{
  int r;
  flagrh = control_readfile(&rh,"control/rcpthosts",0);
  if (flagrh != 1) return flagrh;
  if (!constmap_init(&maprh,rh.s,rh.len,0)) return flagrh = -1;
  r = cdb_init(&db, "control/morercpthosts.cdb");
  if (r == 0) if (errno != ENOENT) return flagrh = -1;
  db_is_loaded = 1;
  return 0;
}

static stralloc host = STRALLOC_ZERO;

int rcpthosts(char *buf, int len)
{
  int j;

  if (flagrh != 1) return 1;

  j = byte_rchr(buf,len,'@');
  if (j >= len) return 1; /* presumably envnoathost is acceptable */

  ++j; buf += j; len -= j;

  if (!stralloc_copyb(&host,buf,len)) return -1;
  buf = host.s;
  case_lowerb(buf,len);

  for (j = 0;j < len;++j)
    if (!j || (buf[j] == '.'))
      if (constmap(&maprh,buf + j,len - j)) return 1;

  if (db_is_loaded) {
    cdb_data data;
    int r;

    for (j = 0;j < len;++j)
      if (!j || (buf[j] == '.')) {
        r = cdb_find(&db, &data, buf + j, len - j);
        if (r == 1) return r;
      }
  }

  return 0;
}
