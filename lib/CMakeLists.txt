execute_process(
    COMMAND  protoc -I=${CMAKE_SOURCE_DIR}/lib/ --cpp_out=${CMAKE_SOURCE_DIR}/lib ${CMAKE_SOURCE_DIR}/lib/variable_type.proto
    OUTPUT_VARIABLE IGNORE
    )
add_library(utils STATIC utils.cpp variable_type.pb.cc)
