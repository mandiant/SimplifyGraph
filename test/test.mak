
TEST32_EXE  := test/test.32.exe
TEST32_OBJS := test/test.32.o SimplifyGraph/FlareGraph.32.o
TEST32_LIBS := -lida

TEST64_EXE  := test/test.64.exe
TEST64_OBJS := test/test.64.o SimplifyGraph/FlareGraph.64.o
TEST64_LIBS := -lida64

$(TEST32_EXE): $(TEST32_OBJS)
	$(LDXX) $(LDXXFLAGS32) -o $@ $^ $(TEST32_LIBS)

$(TEST64_EXE): $(TEST64_OBJS)
	$(LDXX) $(LDXXFLAGS64) -o $@ $^ $(TEST64_LIBS)


OBJS     += $(TEST32_OBJS) $(TEST64_OBJS)
BINARIES += $(TEST32_EXE) $(TEST64_EXE)

test/%.32.o: test/%.cpp
	$(CXX) $(CXXFLAGS32) -c -Itest -ISimplifyGraph -o $@ $<

test/%.64.o: test/%.cpp
	$(CXX) $(CXXFLAGS64) -c -Itest -ISimplifyGraph -o $@ $<

test:: $(TEST32_EXE) $(TEST64_EXE)
	LD_LIBRARY_PATH=$(IDA_SDK)/lib/x86_linux_gcc_32/ test/test.32.exe
	LD_LIBRARY_PATH=$(IDA_SDK)/lib/x86_linux_gcc_64/ test/test.64.exe

