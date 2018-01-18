IDA_SDK ?= $(HOME)/bin/ida-6.95/sdk


CXX       := g++
LDXX      := g++

CXXFLAGS32  := $(CXXFLAGS) -I $(IDA_SDK)/include -m32 -std=c++11 -MMD -fPIC -fpermissive -D__PLUGIN__ -D__IDP__ -D__LINUX__
CXXFLAGS64  := $(CXXFLAGS) -I $(IDA_SDK)/include -m32 -std=c++11 -MMD -fPIC -fpermissive -D__PLUGIN__ -D__IDP__ -D__LINUX__ -D__EA64__
LDXXFLAGS32 := $(LDXXFLAGS) -L $(IDA_SDK)/lib/x86_linux_gcc_32 -m32
LDXXFLAGS64 := $(LDXXFLAGS) -L $(IDA_SDK)/lib/x86_linux_gcc_64 -m32

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

ifneq ($(MAKECMDGOALS),clean)
  -include $(DEPS)
endif

