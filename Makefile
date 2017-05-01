LLVM_CONFIG := llvm-config

CXXFLAGS := -std=c++11 $(shell $(LLVM_CONFIG) --cxxflags) -Wall 
CXXFLAGS += -g -O0 -I./ -Ilib -I${HOME}/Install/protobuf.install/include/ -I$(shell $(LLVM_CONFIG) --includedir) -Wpedantic
LDFLAGS := -L${HOME}/Install/protobuf.install/lib/ -lprotobuf -DGOOGLE_PROTOBUF_NO_RTTI  $(shell $(LLVM_CONFIG) --ldflags --libs --system-libs)
CXX := g++

SOURCE_FILES := dwarf-type-reader.cpp lib/utils.cpp lib/variable_type.pb.cc
PROGRAM := disasm

%.o: %.cpp $(wildcard *.h)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(PROGRAM): $(SOURCE_FILES:%.cpp=%.o)
	$(CXX) -o $(PROGRAM) $^ $(CXXFLAGS) $(LDFLAGS)

clean:
	@rm -rf *.o $(PROGRAM)
