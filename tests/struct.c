// RUN: gcc  %s -g -c  -o %t && ../build/bin/dwarf-type-reader --debug %t 
#include <stdint.h>
typedef struct {
  uint32_t a;
  uint32_t b;
} ms;

int foo(int x) {
  ms LS;
  LS.a = x;
  LS.b = x+1;
  return 0;
}
