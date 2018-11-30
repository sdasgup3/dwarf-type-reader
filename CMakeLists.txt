cmake_minimum_required(VERSION 3.4) 


#project(DwarfReader)
project(dwarf-type-reader)

message(PROJECT_SOURCE_DIR =  ${PROJECT_SOURCE_DIR})  
message(CMAKE_SOURCE_DIR =  ${CMAKE_SOURCE_DIR})  
message(CMAKE_BINARY_DIR = ${CMAKE_BINARY_DIR})  
message(EXECUTABLE_OUTPUT_PATH =  ${EXECUTABLE_OUTPUT_PATH})  
message(EXECUTABLE_LIB_PATH =  ${EXECUTABLE_LIB_PATH})  


# LLVM detection part
# Need to set   LLVM_ROOT during cmake invocation
set(LLVM_ROOT "" CACHE PATH "Root of LLVM install.")
message("LLVM_ROOT = ${LLVM_ROOT}")

# A bit of a sanity check:
if(NOT EXISTS ${LLVM_ROOT}/include/llvm )
    message(FATAL_ERROR
            "LLVM_ROOT (${LLVM_ROOT}) is invalid")
endif()

list(APPEND CMAKE_PREFIX_PATH "${LLVM_ROOT}/lib/share/cmake")
find_package(LLVM REQUIRED CONFIG)

message(LLVM_FOUND=${LLVM_FOUND})
message(LLVM_CMAKE_DIR=${LLVM_CMAKE_DIR})

# AddLLVM needs these
set(LLVM_RUNTIME_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}//bin)
set(LLVM_LIBRARY_OUTPUT_INTDIR ${CMAKE_BINARY_DIR}//lib)  

list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
include(HandleLLVMOptions) # load additional config
include(AddLLVM) # used to add our own modules

# propagate LLVM-specific variables to this project
add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})

message(LLVM_DEFINITIONS=${LLVM_DEFINITIONS})
message(LLVM_INCLUDE_DIRS=${LLVM_INCLUDE_DIRS})
message(CMAKE_CURRENT_SOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR})

# Following commands are specific to this project
include_directories(${CMAKE_SOURCE_DIR}/lib)
include_directories($ENV{HOME}/Install/protobuf.install/include/)

add_subdirectory(${CMAKE_SOURCE_DIR}/lib)

set(LLVM_LINK_COMPONENTS
  ${LLVM_TARGETS_TO_BUILD}
  IRReader
  BitReader
  BitWriter
  Core
  Support
  DebugInfoDWARF
)


# Adds the -L flag
link_directories($ENV{HOME}/Install/protobuf.install/lib/)

add_llvm_tool(dwarf-type-reader 
    dwarf-type-reader.cpp
)
# Adds the -lprotobuf
#target_link_libraries(dwarf-type-reader utils protobuf)

# OR
target_link_libraries(dwarf-type-reader PRIVATE utils
	$ENV{HOME}/Install/protobuf.install/lib/libprotobuf.a)


list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CheckFormat.cmake)






