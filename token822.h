#ifndef TOKEN822_H
#define TOKEN822_H
#include <skalibs/genalloc.h>

struct token822
 {
  int type;
  char *s;
  int slen;
 }
;

#define token822_s(a) genalloc_s(struct token822, a)
#define token822_ready(a, n) genalloc_ready(struct token822, a, n)
#define token822_readyplus(a, n) genalloc_ready(struct token822, a, n)

#define token822_append(a, e) genalloc_append(struct token822, a, e)

#define token822_len(a) genalloc_len(struct token822, a)
#define token822_setlen(a, n) genalloc_setlen(struct token822, a, n)
#define token822_deltalen(a, d) genalloc_setlen(struct token822, a,\
        genalloc_len(struct token822, a) + d)

extern int token822_parse();
extern int token822_addrlist();
extern int token822_unquote();
extern int token822_unparse();
extern void token822_free();
extern void token822_reverse();

#define TOKEN822_ATOM 1
#define TOKEN822_QUOTE 2
#define TOKEN822_LITERAL 3
#define TOKEN822_COMMENT 4
#define TOKEN822_LEFT 5
#define TOKEN822_RIGHT 6
#define TOKEN822_AT 7
#define TOKEN822_COMMA 8
#define TOKEN822_SEMI 9
#define TOKEN822_COLON 10
#define TOKEN822_DOT 11

#endif
