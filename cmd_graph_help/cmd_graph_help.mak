
CGH32_EXE  := cmd_graph_help/cmd_graph_help.32.exe
CGH32_OBJS := cmd_graph_help/cmd_graph_help.32.o \
              SimplifyGraph/FlareGraph.32.o
CGH32_LIBS := -lida

CGH64_EXE  := cmd_graph_help/cmd_graph_help.64.exe
CGH64_OBJS := cmd_graph_help/cmd_graph_help.64.o \
              SimplifyGraph/FlareGraph.64.o
CGH64_LIBS := -lida64

$(CGH32_EXE): $(CGH32_OBJS)
	$(LDXX) $(LDXXFLAGS32) -o $@ $^ $(CGH32_LIBS)

$(CGH64_EXE): $(CGH64_OBJS)
	$(LDXX) $(LDXXFLAGS64) -o $@ $^ $(CGH64_LIBS)


OBJS     += $(CGH64_OBJS)
BINARIES += $(CGH64_EXE)

cmd_graph_help/%.32.o: cmd_graph_help/%.cpp
	$(CXX) $(CXXFLAGS32) -c -Icmd_graph_help -ISimplifyGraph -o $@ $<

cmd_graph_help/%.64.o: cmd_graph_help/%.cpp
	$(CXX) $(CXXFLAGS64) -c -Icmd_graph_help -ISimplifyGraph -o $@ $<

