// RUN: gcc  %s -g -c -o %t && ../build/bin/dwarf-type-reader --debug %t
#include<iostream>
struct biff
{
   int a;
        struct biff *ptr;
} ;

int main()
{
  struct biff *B = new struct biff;
  B->ptr = NULL;
}
