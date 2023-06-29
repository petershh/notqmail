#include <skalibs/types.h>
#include <skalibs/buffer.h>

int main(int argc, char **argv)
{
  char *name;
  char *value;
  unsigned long num;
  char strnum[ULONG_FMT];

  if (argc != 3)
      return 100;
  name = argv[1];
  if (!name)
      return 100;
  value = argv[2];
  if (!value)
      return 100;

  ulong_oscan(value, &num);
  strnum[ulong_fmt(strnum,num)] = 0;

  buffer_puts(buffer_1small, "int ");
  buffer_puts(buffer_1small, name);
  buffer_puts(buffer_1small, " = ");
  buffer_puts(buffer_1small, strnum);
  buffer_puts(buffer_1small, ";\n");
  if (buffer_flush(buffer_1small) == -1) return 111;
  return 0;
}
