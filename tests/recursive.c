// RUN: gcc  %s -g -c -o %t && ../build/bin/dwarf-type-reader --debug %t
struct biff
{
        //struct biff *ptr[2];
        struct biff *ptr;
} ;

int foo(struct biff baz)
{
}
