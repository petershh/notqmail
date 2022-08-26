#ifndef QMAIL_H
#define QMAIL_H

#include <string.h>

#include <sys/uio.h>

#include <skalibs/buffer.h>
#include <skalibs/functypes.h>

#include "substdio.h"

struct qmail {
  int flagerr;
  unsigned long pid;
  int fdm;
  int fde;
  int fderr;
  buffer b;
  char buf[1024];
} ;

extern int qmail_open();
extern void qmail_put();
#define qmail_puts(qq,s) qmail_put(qq,s,strlen(s))
extern void qmail_from();
extern void qmail_to();
extern void qmail_fail();
extern char *qmail_close();
extern unsigned long qmail_qp();

#define GEN_QMAILPUT_WRITE(qq) \
ssize_t qmail_put_write(int fd, struct iovec const *v, unsigned int len) \
{ \
  qmail_put(qq, v, len); \
  return len; \
}

#endif
