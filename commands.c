#include <stddef.h>

#include <skalibs/stralloc.h>
#include <skalibs/buffer.h>
#include <skalibs/djbunix.h>
#include <skalibs/bytestr.h>

#include "commands.h"

void execute_command(char *line, size_t len, struct command const *cmds)
{
  size_t whitespace_pos, i;
  char *arg;
  if (line[len - 1] == '\r')
    len--;

  line[len] = '\0';
  whitespace_pos = str_chr(line, ' ');
  arg = line + whitespace_pos;
  while (*arg == ' ')
    arg++;

  for (i = 0; cmds[i].text; i++) {
    if (case_equalb(cmds[i].text, whitespace_pos, line))
      break;
  }
  cmds[i].fun(arg, cmds[i].data);
  if (cmds[i].flush) /* TODO does it belong here? */
    cmds[i].flush();
}
