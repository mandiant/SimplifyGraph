
TEST_EXE  := test/test.exe
TEST_OBJS := test/test.o SimplifyGraph/FlareGraph.o
TEST_LIBS := -lida

$(TEST_EXE): $(TEST_OBJS)
	$(LDXX) $(LDXXFLAGS) -o $@ $^ $(TEST_LIBS)


OBJS     += $(TEST_OBJS)
BINARIES += $(TEST_EXE)

test/%.o: test/%.cpp
	$(CXXC) $(CXXFLAGS) -c -Itest -ISimplifyGraph -o $@ $<

