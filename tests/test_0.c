// RUN: gcc  %s -g -c -o %t && ../build/bin/dwarf-type-reader %t 

typedef int MI;
int foo(MI asI) { 
  register int lsI  = 0 ;
  unsigned int lauI[] = {2,3};
  int *lpsI = &asI;
  return lauI[1] + *lpsI;
}

int main() {
  int main_x = 100;
  foo(main_x);
  return 0;
}
