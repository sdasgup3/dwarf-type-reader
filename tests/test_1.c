// RUN: gcc  %s -g -c -o %t && ../build/bin/dwarf-type-reader %t
int a;
int foo(int c) { 
   int b[2];

  for(int i = 0 ; i < 1000; i++) {
    b[i%2] = b[(i+1)%2] - c;
  }

  return b[0];
}

int main() {
  a = 10;
  foo(a);

  return 0;
}
