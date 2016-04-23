#include <stdio.h>
#define FALSE 0
#define TRUE 1

main() { /* wordcount program */
  int nl=0, nw=0, nc=0;
  int inword = FALSE;
  char c;

  c = getchar();
  while (c != EOF) {
    ++nc;
    if (c == '\n') { ++nl; }
    if (c == ' ' || c == '\n' || c == '\t')
      { inword = FALSE; }
    else if (!inword) {
      inword = TRUE;
      ++nw;
    }
    c = getchar();
  }
  printf("%d lines, %d words, %d chars\n", nl, nw, nc);
}

