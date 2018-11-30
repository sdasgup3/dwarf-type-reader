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
  cmake -DCMAKE_BUILD_TYPE=Debug -DLLVM_ROOT=/home/sdasgup3/Install/llvm.release.install/ ..
  make dwarf-type-reader
```
## Running the code On Linux

```shell
  ./build/bin/dwarf-type-reader [--debug] <binary file with debg info>
```
## Runnung tests
```shell
cd tests
lit .
```

## Example 1
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
  
  name = "baz"
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
}

```
## Example 2

For recursive struct
```C
struct biff
{
        struct biff *ptr;
} ;

int foo(struct biff baz)
{
}
```
Output:
```
stack_variables {
  var {
    name: "baz"
    type {
      size: 8
      c_type: "struct biff"
      kind: isStruct
      member_list {
        field_offset: 0
        field_type {
          size: 8
          c_type: "* struct biff"
          kind: isPointer
          element_type {
            size: 8
            c_type: "struct biff"
            kind: isStruct
	    /************** NO FURTHER MEMBER LIST IS EXTRACTED (otherwise will get into inf loop) ***********/
          }
        }
        field_name: "ptr"
      }
    }
  }
  scope {
    entry_address: 0
    symbol_name: "foo"
  }
  is_formal_parameter: true
}
```
### Comment
The dwarf variables shares the same die to represent types. For example two different variables (or `DW_TAG_variable` dies' ) of types `int` will re-use the same `DW_TAG_base_type` type die (at the same offset in the dwarf dump) to represent that they are of the same type. To avoid redundant searching of these type dies' for variable of same type , we store these type dies in a map with key equal to the offset of the type die and value as the type information extracted from that die.

For determining the type of any variable we follow:
 1. First check  if the type of the variable is already extracted ( i.e the corresponding type die is available in the map)
 2. If not, then extract the information from the corresponding type die.

For recursive type, we cannot wait for the entire type (means struct type with the type of all the fields) because it is not possible to syntactically unroll the entire structure. 

So once we extract the name of the struct `e.g. struct biff`, we populate that information  in the map. Then while we traverse for the recursive member field `e.g. struct biff *ptr`, we dont have to extract it again as it will be avaialble in the map. That is the reason that for a recursive member, the member list is absent.





