typedef int MI;
int foo(MI asI) { 
  register int lsI  = 0 ;
  unsigned int lauI[] = {2,3};
  int *lpsI = &asI;
  return lauI[1] + *lpsI;
}
