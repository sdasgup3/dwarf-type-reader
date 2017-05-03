// RUN: gcc  %s -g -c -o %t && ../build/bin/dwarf-type-reader %t

#define __FLEXIBLE_ARRAY_MEMBER

struct _obstack_chunk           /* Lives at front of each chunk. */
{
  char *limit;                  /* 1 past end of this chunk */
  struct _obstack_chunk *prev;  /* address of prior chunk or NULL */
  char contents[__FLEXIBLE_ARRAY_MEMBER]; /* objects begin here */
};

void foo(struct _obstack_chunk xxx) {

}

