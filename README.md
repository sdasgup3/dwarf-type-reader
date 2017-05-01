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
  mkdir build; cd  build
  cmake -DLLVM_ROOT=/home/sdasgup3/Install/llvm.release.install/ ..
  make dwarf-type-reader
```
## Building the code On Linux

```shell
  ./build/bin/dwarf-type-reader [--debug] <binary file with debg info>
```


## Example
For the toy C program mytest.o
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
Output of the tool:
 - **mytest.o.debuginfo** The Variable and correcponding type information dumped as protobuf binary based on the [proto definition file](lib/variable_type.proto)
 - The **following ouput** is emitted by the tool just for the debug purpose.
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
