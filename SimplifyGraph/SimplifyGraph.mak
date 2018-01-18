
SIMPLIFY_EXE  := SimplifyGraph/libSimplifyGraph.so
SIMPLIFY_OBJS := SimplifyGraph/FlareGraph.o \
                 SimplifyGraph/SimplifyGraph.o
SIMPLIFY_LIBS := -lida


$(SIMPLIFY_EXE): $(SIMPLIFY_OBJS)
	$(LDXX) $(LDXXFLAGS) --shared -o $@ $^ $(SIMPLIFY_LIBS)


OBJS     += $(SIMPLIFY_OBJS)
BINARIES += $(SIMPLIFY_EXE)

SimplifyGraph/%.o: SimplifyGraph/%.cpp
	$(CXXC) $(CXXFLAGS) -c -ISimplifyGraph -o $@ $<

