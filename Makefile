IDA_DIR ?= $(HOME)/bin/ida-6.95/sdk


CXXC      := g++
LDXX      := g++

CXXFLAGS  := $(CXXFLAGS) -I $(IDA_DIR)/include -m32 -std=c++11 -MMD -fPIC -D__LINUX__ -fpermissive
LDXXFLAGS := $(LDXXFLAGS) -L $(IDA_DIR)/lib/x86_linux_gcc_32 -m32

.PHONY: all bin clean test
.DEFAULT_GOAL := all

BINARIES :=
OBJS     :=

include test/test.mak
include SimplifyGraph/SimplifyGraph.mak
include cmd_graph_help/cmd_graph_help.mak

OBJS := $(sort $(OBJS))
DEPS := $(OBJS:.o=.d)


all: bin
 
bin: $(BINARIES)

clean:
	rm -f $(DEPS)
	rm -f $(OBJS)
	rm -f $(BINARIES)

test: test/test.exe
	LD_LIBRARY_PATH=$(IDA_DIR)/lib/x86_linux_gcc_32/ test/test.exe

ifneq ($(MAKECMDGOALS),clean)
  -include $(DEPS)
endif

