
SIMPLIFY32_EXE  := SimplifyGraph/SimplifyGraph.plx
SIMPLIFY32_OBJS := SimplifyGraph/FlareGraph.32.o \
                   SimplifyGraph/SimplifyGraph.32.o

SIMPLIFY64_EXE  := SimplifyGraph/SimplifyGraph.plx64
SIMPLIFY64_OBJS := SimplifyGraph/FlareGraph.64.o \
                   SimplifyGraph/SimplifyGraph.64.o
SIMPLIFY64_LIBS := -lida64


$(SIMPLIFY32_EXE): $(SIMPLIFY32_OBJS)
	$(LDXX) $(LDXXFLAGS32) --shared -o $@ $^ $(SIMPLIFY32_LIBS)

$(SIMPLIFY64_EXE): $(SIMPLIFY64_OBJS)
	$(LDXX) $(LDXXFLAGS64) --shared -o $@ $^ $(SIMPLIFY64_LIBS)


OBJS     += $(SIMPLIFY32_OBJS) $(SIMPLIFY64_OBJS)
BINARIES += $(SIMPLIFY32_EXE) $(SIMPLIFY64_EXE)

SimplifyGraph/%.32.o: SimplifyGraph/%.cpp
	$(CXX) $(CXXFLAGS32) -c -ISimplifyGraph -o $@ $<

SimplifyGraph/%.64.o: SimplifyGraph/%.cpp
	$(CXX) $(CXXFLAGS64) -c -ISimplifyGraph -o $@ $<

