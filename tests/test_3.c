// RUN: gcc  %s -g -c -o %t && ../build/bin/dwarf-type-reader %t
int foo(int first, int m)
{
	int bar;
        bar = first+m;
        {
	  int bar;
          bar = first+m;
        }
	return bar;
}

int main(int argc, char **argv)
{
	int m=10;
	m = foo(11, m);

	return 0;
}
