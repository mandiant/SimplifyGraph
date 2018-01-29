IDA_SDK ?= $(HOME)/bin/ida-6.95/sdk
IDA_DIR ?= $(HOME)/bin/ida-6.95


CXX       := g++
LDXX      := g++

NATIVE_ARCH := $(shell uname -m)
TARGET_ARCH ?= i686

ifneq ($(NATIVE_ARCH),$(TARGET_ARCH))
ifeq ($(TARGET_ARCH),i686)
  ARCH_FLAG := -m32
else
  ARCH_FLAG :=
endif
endif

CXXFLAGS32  := $(CXXFLAGS) -I $(IDA_SDK)/include $(ARCH_FLAG) -std=c++11 -MMD -fPIC -fpermissive -D__PLUGIN__ -D__IDP__ -D__LINUX__
CXXFLAGS64  := $(CXXFLAGS) -I $(IDA_SDK)/include $(ARCH_FLAG) -std=c++11 -MMD -fPIC -fpermissive -D__PLUGIN__ -D__IDP__ -D__LINUX__ -D__EA64__
LDXXFLAGS32 := $(LDXXFLAGS) -L $(IDA_SDK)/lib/x86_linux_gcc_32 $(ARCH_FLAG)
LDXXFLAGS64 := $(LDXXFLAGS) -L $(IDA_SDK)/lib/x86_linux_gcc_64 $(ARCH_FLAG)

.PHONY: all bin clean install test
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

