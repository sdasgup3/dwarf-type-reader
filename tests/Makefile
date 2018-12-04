LLVM_CONFIG := llvm-config

CFLAGS :=  -Wall -g -O0 --static
CXXFLAGS := -std=c++11 -Wall -g -O0 --static
CXX := g++
CC := gcc
DISASM := objdump -d
TYPEBIN := ../disasm
DWARFDUMP := llvm-dwarfdump

SOURCES=$(wildcard *.c)
OBJECTS := $(patsubst %.c, %.o, $(SOURCES))

OBJDUMPS := $(patsubst %.c, %.objdump, $(SOURCES))
TYPEINFO := $(patsubst %.c, %.typeinfo, $(SOURCES))
DWARFINFO := $(patsubst %.c, %.dwarfinfo, $(SOURCES))

all:${OBJECTS} ${OBJDUMPS} ${TYPEINFO} ${DWARFINFO}

%.o: %.c
	$(CC) $(CFLAGS)  -o $@ $<

%.o: %.cc
	$(CXX) $(CXXFLAGS) -o $@ $<

%.objdump: %.o
	$(DISASM) $< &> $@ 

%.typeinfo: %.o
	$(TYPEBIN) $< &> $@ 

%.dwarfinfo: %.o
	$(DWARFDUMP) $< &> $@ 

.PHONY: clean

clean:
	@rm -rf ${OBJECTS} ${OBJDUMPS} ${TYPEINFO} ${DWARFINFO}
