#ifndef SPAWN_H
#define SPAWN_H

#include <skalibs/buffer.h>

extern int truncreport;
extern int spawn(int fdmess, int fdout, char *s, char *r, int at);
extern void report(buffer *b, int wstat, char *s, int len);
extern void initialize(int argc, char **argv);

#endif
