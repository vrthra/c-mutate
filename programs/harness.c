#include "config.h"

#define TSIZE 20
#define PSIZE 15

int covered = 0;

int nondet_int();

int main () {
  char text1[TSIZE];
  char pat1[PSIZE];
  
  int tlen1 = nondet_int();
  int plen1 = nondet_int();
  int s1 = nondet_int();

  char text2[TSIZE];
  char pat2[PSIZE];
  
  int tlen2 = nondet_int();
  int plen2 = nondet_int();
  int s2 = nondet_int();

  int r1 = js_BoyerMooreHorspool(text1, tlen1, pat1, plen1, s1);
  int r2 = js_BoyerMooreHorspool43(text1, tlen1, pat1, plen1, s1);

  int covered1 = covered;
  covered = 0;

  int r3 = js_BoyerMooreHorspool(text2, tlen2, pat2, plen2, s2);
  int r4 = js_BoyerMooreHorspool43(text2, tlen2, pat2, plen2, s2);

  int covered2 = covered;

  int not_found = covered1 && (r1 == r2);
  int found = covered2 && (r3 != r4);

  assert (! (found && not_found));
}
