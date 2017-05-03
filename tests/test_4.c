// RUN: gcc  %s -g -c -o %t && ../build/bin/dwarf-type-reader %t
int a;
int foo(int c) { 
  register int b[2];

  return b[0] + c - b[1];
}
