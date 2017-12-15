// cmd_graph_help.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <fstream>
#include <sstream>

#include "FlareGraph.hpp"

static const char* const g_ColorNames[] = {
"aliceblue",
"antiquewhite",
"antiquewhite1",
"antiquewhite2",
"antiquewhite3",
"antiquewhite4",
"aquamarine",
"aquamarine1",
"aquamarine2",
"aquamarine3",
"aquamarine4",
"azure",
"azure1",
"azure2",
"azure3",
"azure4",
"beige",
"bisque",
"bisque1",
"bisque2",
"bisque3",
"bisque4",
"black",
"blanchedalmond",
"blue",
"blue1",
"blue2",
"blue3",
"blue4",
"blueviolet",
"brown",
"brown1",
"brown2",
"brown3",
"brown4",
"burlywood",
"burlywood1",
"burlywood2",
"burlywood3",
"burlywood4",
"cadetblue",
"cadetblue1",
"cadetblue2",
"cadetblue3",
"cadetblue4",
"chartreuse",
"chartreuse1",
"chartreuse2",
"chartreuse3",
"chartreuse4",
"chocolate",
"chocolate1",
"chocolate2",
"chocolate3",
"chocolate4",
"coral",
"coral1",
"coral2",
"coral3",
"coral4",
"cornflowerblue",
"cornsilk",
"cornsilk1",
"cornsilk2",
"cornsilk3",
"cornsilk4",
"crimson",
"cyan",
"cyan1",
"cyan2",
"cyan3",
"cyan4",
"darkgoldenrod",
"darkgoldenrod1",
"darkgoldenrod2",
"darkgoldenrod3",
"darkgoldenrod4",
"darkgreen",
"darkkhaki",
"darkolivegreen",
"darkolivegreen1",
"darkolivegreen2",
"darkolivegreen3",
"darkolivegreen4",
"darkorange",
"darkorange1",
"darkorange2",
"darkorange3",
"darkorange4",
"darkorchid",
"darkorchid1",
"darkorchid2",
"darkorchid3",
"darkorchid4",
"darksalmon",
"darkseagreen",
"darkseagreen1",
"darkseagreen2",
"darkseagreen3",
"darkseagreen4",
"darkslateblue",
"darkslategray",
"darkslategray1",
"darkslategray2",
"darkslategray3",
"darkslategray4",
"darkslategrey",
"darkturquoise",
"darkviolet",
"deeppink",
"deeppink1",
"deeppink2",
"deeppink3",
"deeppink4",
"deepskyblue",
"deepskyblue1",
"deepskyblue2",
"deepskyblue3",
"deepskyblue4",
"dimgray",
"dimgrey",
"dodgerblue",
"dodgerblue1",
"dodgerblue2",
"dodgerblue3",
"dodgerblue4",
"firebrick",
"firebrick1",
"firebrick2",
"firebrick3",
"firebrick4",
"floralwhite",
"forestgreen",
"gainsboro",
"ghostwhite",
"gold",
"gold1",
"gold2",
"gold3",
"gold4",
"goldenrod",
"goldenrod1",
"goldenrod2",
"goldenrod3",
"goldenrod4"
};

static const int g_ColorNamesCount = sizeof(g_ColorNames)/sizeof(g_ColorNames[0]);

void printUsage(TCHAR* name) {
    printf("Usage: %s <cmd> <input_file> <output_file>\n", name);
    printf("Where <cmd> is one of UR or PD or CT\n");
}

void handleCT(TCHAR* ifilename, TCHAR* ofilename) {
    std::ifstream ifile(ifilename);

    if (!ifile) {
        _tprintf(_T("failed to open input file '%s'\n"), ifilename);
        return;
    }
    Flare::FlareGraph fg;
    bool ret = fg.loadGraphViz(ifile);
    ifile.close();
    if (!ret) {
        printf("Failed to load dot file\n");
        return;
    }

    Flare::intintMap_t idomtree;
    ret = fg.getDomIntMap(idomtree, false);
    if (!ret) {
        printf("getDomIntMap failed :(\n");
        return;
    }
    printf("idomtree:\n");
    for (auto it = idomtree.begin(); it != idomtree.end(); ++it) {
        printf("idomtree[%3d] = %3d\n", it->first, it->second);
    }
    Flare::intintMap_t ipdomtree;
    ret = fg.getDomIntMap(ipdomtree, true);
    if (!ret) {
        printf("getDomIntMap pd failed :(\n");
        return;
    }
    printf("ipdomtree:\n");
    for (auto it = ipdomtree.begin(); it != ipdomtree.end(); ++it) {
        printf("ipdomtree[%3d] = %3d\n", it->first, it->second);
    }

    Flare::subgraphVec_t subgraphs;
    ret = fg.findSimpleSubGraphs(subgraphs, 3, 95);
    if (!ret) {
        printf("findSimpleSubGraphs failed :(\n");
        return;
    }
    printf("Found %d interesting subgraphs\n", subgraphs.size());
    for (auto it = subgraphs.begin(); it != subgraphs.end(); ++it) {
        printf("Got interesting subgraph <%d,%d>: %d nodes\n", it->n1, it->n2, it->nodes.size());
        for (auto jt = it->nodes.begin(); jt != it->nodes.end(); ++jt) {
            printf("    %3d\n", *jt);
        }
    }
#if 0
    Flare::intintMap_t ipdomMap;
    ret = fg.getDomIntMap(ipdomMap, true);
    if (!ret) {
        printf("Failed get ipdom map\n");
        return;
    }

    Flare::intintMap_t idomMap;
    ret = fg.getDomIntMap(idomMap, false);
    if (!ret) {
        printf("Failed get idom map\n");
        return;
    }

    printf("Got ipdom map with %d entries\n", ipdomMap.size());
    for (auto pit = ipdomMap.begin(); pit != ipdomMap.end(); ++pit) {
        printf("ipdom[%d] = %d\n", pit->first, pit->second);
    }
    printf("Got idom map with %d entries\n", idomMap.size());
    for (auto dit = idomMap.begin(); dit != idomMap.end(); ++dit) {
        printf("idom[%d] = %d\n", dit->first, dit->second);
    }

    int colorIdx = 0;

    for (auto pit = ipdomMap.begin(); pit != ipdomMap.end(); ++pit) {
        auto dit = idomMap.find(pit->second);
        if (dit == idomMap.end()) {
            printf("Not sure if this an error or not: failed to find %d in the idomMap\n", pit->first);
            continue;
        }
        if ((dit->second == pit->first) && (dit->first == pit->second)) {
            colorIdx = rand() % g_ColorNamesCount;
            printf("Matching idom/ipdom  nodes %d <=> %d: %s\n", pit->first, dit->first, g_ColorNames[colorIdx]);
            fg.colorNode(pit->first, g_ColorNames[colorIdx]);
        }

    }
#endif
#if 0
    int colorIdx = 0;
    for (auto mit = ipdomSetMap.begin(); mit != ipdomSetMap.end(); ++mit) {
        colorIdx = rand() % g_ColorNamesCount;
        //printf("Using color %d of %d: %s\n", colorIdx, g_ColorNamesCount, g_ColorNames[colorIdx]);
        for (auto sit = mit->second.begin(); sit != mit->second.end(); ++sit) {
            printf("%d postdominates %d: %s\n", mit->first, *sit, g_ColorNames[colorIdx]);
            fg.colorNode(*sit, g_ColorNames[colorIdx]);
        }
        //wrap & reuse. don't try to do anything fancy to prevent adjacent colors
        //++colorIdx;
        //if (colorIdx >= g_ColorNamesCount) {
        //    colorIdx = 0;
        //}
    }
#endif
    std::ostringstream ostringstr;
    fg.storeGraphViz(ostringstr);
    FILE *outf = _tfopen(ofilename, _T("w"));
    std::string outs = ostringstr.str();
    fwrite(outs.c_str(), 1, outs.size(), outf);
    fclose(outf);
    printf("Done\n");
}

void handleUR(TCHAR* ifilename, TCHAR* ofilename) {
    std::ifstream ifile(ifilename);

    if (ifile) {
#if 0
        std::stringstream buffer;
        buffer << ifile.rdbuf();
        std::string input;
        input = buffer.str();
        std::istringstream is(input);

        printf("Read input file\n%s\n", input.c_str());
        ifile.seekg(0, is.beg);
#endif
        Flare::FlareGraph fg;
        bool ret = fg.loadGraphViz(ifile);
        ifile.close();
        if (!ret) {
            printf("Failed to load dot file\n");
            return;
        }
        Flare::intset_t ureach;
        Flare::intset_t reach;
        ret = fg.reachable(reach);
        if (!ret) {
            printf("Failed to get reachable\n");
            return;
        }
        ret = fg.uniqueReachable(ureach);
        if (!ret) {
            printf("Failed to get unique reachable\n");
            return;
        }
        if (ureach.size() == 0) {
            printf("Bailing out. Received 0 nodes for new group\n");
            return;
        }
        printf("Got %d unique reachable nodes\n", ureach.size());
        for (auto it = reach.begin(); it != reach.end(); ++it) {
            if (*it == fg.getRootNode()) {
                // pass
                printf("Skipping root node %d %d\n", *it, fg.getRootNode());
            } else if (ureach.find(*it) == ureach.end()) {
                // reachable, but not in unique reachable -> pruned
                fg.addNodeProp(*it, Flare::NodeProperty::Pruned);
            } else {
                fg.addNodeProp(*it, Flare::NodeProperty::Selected);
            }
        }

        std::ostringstream ostringstr;
        fg.storeGraphViz(ostringstr);
        FILE *outf = _tfopen(ofilename, _T("w"));
        std::string outs = ostringstr.str();
        fwrite(outs.c_str(), 1, outs.size(), outf);
        fclose(outf);
        printf("Done\n");

    }

}

void handlePD(TCHAR* ifilename, TCHAR* ofilename) {
    std::ifstream ifile(ifilename);

    if (!ifile) {
        _tprintf(_T("failed to open input file '%s'\n"), ifilename);
        return;
    }
    Flare::FlareGraph fg;
    bool ret = fg.loadGraphViz(ifile);
    ifile.close();
    if (!ret) {
        printf("Failed to load dot file\n");
        return;
    }
    printf("Doing idom stuff starting at %d\n", fg.getRootNode());
    Flare::intset_t pdomgroup;
    ret = fg.getpdomGroup(pdomgroup, fg.getRootNode());
    if (!ret) {
        printf("Error getting pdomgroup\n");
        return;
    }
    printf("Marking up %d selected nodes\n", pdomgroup.size());
    for (auto it = pdomgroup.begin(); it != pdomgroup.end(); ++it) {
        if (*it == fg.getRootNode()) {
            // pass
            printf("Skipping root node %d %d\n", *it, fg.getRootNode());
        //} else if (ureach.find(*it) == ureach.end()) {
        //    // reachable, but not in unique reachable -> pruned
        //    fg.addNodeProp(*it, Flare::NodeProperty::Pruned);
        } else {
            printf("Marking up selected node %d\n", *it);
            fg.addNodeProp(*it, Flare::NodeProperty::Selected);
        }
    }

    std::ostringstream ostringstr;
    fg.storeGraphViz(ostringstr);
    FILE *outf = _tfopen(ofilename, _T("w"));
    std::string outs = ostringstr.str();
    fwrite(outs.c_str(), 1, outs.size(), outf);
    fclose(outf);
    printf("Done\n");
}


int _tmain(int argc, _TCHAR* argv[]) {
    printf("Hello World\n");
    if (argc != 4) {
        printf("Missing command line parameters");
        printUsage(argv[0]);
        return 1;
    }
    bool doUR = false;
    bool doPD = false;
    bool doCT = false;
    if (_tcscmp(argv[1], _T("UR")) == 0) {
        doUR = true;
    } else if (_tcscmp(argv[1], _T("PD")) == 0) {
        doPD = true;
    } else if (_tcscmp(argv[1], _T("CT")) == 0) {
        doCT = true;
    } else {
        printf("Incorrect <cmd>: '%s'\n", argv[1]);
        printUsage(argv[0]);
        return 1;
    }
    if (doUR) {
        handleUR(argv[2], argv[3]);
    } else if (doPD) {
        handlePD(argv[2], argv[3]);
    } else if (doCT) {
        handleCT(argv[2], argv[3]);
    }

    return 0;
}

