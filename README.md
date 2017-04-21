# dwarf-type-reader

To read type information from debug info section of executable usig LLVM based APIs.

## Dependencies

| Name | Version | 
| ---- | ------- |
| [Git](https://git-scm.com/) | Latest |
| [CMake](https://cmake.org/) | 3.1+ |
| [Google Protobuf](https://github.com/google/protobuf) | 2.6.1 |
| [LLVM](http://llvm.org/) | 5.0.0 | 

## Building the code On Linux
```shell
  make
```
## Example
For the toy C program
```C
struct biff
{
	int i;
        char c[2];
} ;

int foo(struct biff **baz)
{
}
```
Output of the tool
```C
local_variables {
  scope {
    symbol_name: "foo"
  }
  is_formal_parameter: true
  type {
    size: 8
    c_type: " ** struct biff"
    kind: isPointer
    element_type {
      size: 8
      c_type: " * struct biff"
      kind: isPointer
      element_type {
        size: 8
        c_type: "struct biff"
        kind: isStruct
        member_list {
          field_offset: 0
          field_type {
            size: 4
            c_type: "signed"
            kind: isScalar
          }
          field_name: "i"
        }
        member_list {
          field_offset: 4
          field_type {
            size: 2
            c_type: "signed_char[2]"
            kind: isArray
            element_type {
              size: 1
              c_type: "signed_char"
              kind: isScalar
            }
          }
          field_name: "c"
        }
      }
    }
  }
  name: "baz"
}

```
