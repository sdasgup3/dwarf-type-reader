LLVM_CONFIG := llvm-config

CXXFLAGS := -std=c++11 $(shell $(LLVM_CONFIG) --cxxflags) -Wall 
CXXFLAGS += -g -O0 -I./
LDFLAGS := $(shell $(LLVM_CONFIG) --ldflags --libs --system-libs)
CXX := g++

SOURCE_FILES := dwarf-type-reader.cpp utils.cpp
PROGRAM := dwarf-type-reader

%.o: %.cpp $(wildcard *.h)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

$(PROGRAM): $(SOURCE_FILES:%.cpp=%.o)
	$(CXX) -o $(PROGRAM) $^ $(CXXFLAGS) $(LDFLAGS)

clean:
	@rm -rf *.o $(PROGRAM)
