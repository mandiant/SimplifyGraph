
CGH_EXE  := cmd_graph_help/cmd_graph_help.exe
CGH_OBJS := cmd_graph_help/cmd_graph_help.o \
            SimplifyGraph/FlareGraph.o
CGH_LIBS := -lida

$(CGH_EXE): $(CGH_OBJS)
	$(LDXX) $(LDXXFLAGS) -o $@ $^ $(CGH_LIBS)


OBJS     += $(CGH_OBJS)
BINARIES += $(CGH_EXE)

cmd_graph_help/%.o: cmd_graph_help/%.cpp
	$(CXXC) $(CXXFLAGS) -c -Icmd_graph_help -ISimplifyGraph -o $@ $<

