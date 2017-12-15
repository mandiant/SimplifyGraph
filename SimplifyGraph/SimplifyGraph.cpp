///////////////////////////////////////////////////////////////////////////////
// 
//  Copyright (C) 2017 FireEye, Inc. All Rights Reserved.
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.

#include "stdafx.h"
#pragma warning(disable: 4996)


#if IDA_SDK_VERSION < 700

#define STARTEA     startEA
#define ENDEA       endEA
#define UI_POPULATING_POPUP ui_populating_tform_popup
#define GET_TFORM_TYPE get_tform_type
#define PROCESS_FLOWCHART processor_t::preprocess_chart
#define RUN_RET_TYPE void
#define RUN_RET 
#define RUN_ARG_TYPE int
#define CHOOSER_PARENT

#else // #if IDA_SDK_VERSION < 700

#define STARTEA     start_ea
#define ENDEA       end_ea
#define UI_POPULATING_POPUP ui_populating_widget_popup
#define GET_TFORM_TYPE get_widget_type
#define PROCESS_FLOWCHART idb_event::flow_chart_created
#define RUN_RET_TYPE bool
#define RUN_RET true
#define RUN_ARG_TYPE size_t
#define CHOOSER_PARENT : public chooser_t

#endif // #if IDA_SDK_VERSION < 700


//EAFMTSTR : format string for EAs, depending on 32/64 IDA
#ifdef __EA64__
#define EAFMTSTR  "llx"
#else  // #ifdef __EA64__
#define EAFMTSTR  "x"
#endif // #ifdef __EA64__

#include "FlareGraph.hpp"
#include "idalogging.h"

#define ACTION_NAME_COLLAPSE_UNIQUE_REACH "SimplifyGraph:collapse_unique_reachable"
#define ACTION_NAME_COLLAPSE_POST_DOM "SimplifyGraph:collapse_post_dom"
#define ACTION_NAME_CREATE_DOTFILE "SimplifyGraph:create_dot_file"
#define ACTION_NAME_COLLAPSE_SWITCH_CASES "SimplifyGraph:collapse_ur_switch_cases"
#define ACTION_NAME_DISCOVER_ISOLATED_SUBGRAPHS "SimplifyGraph:discover_isolated_subgraphs"
#define ACTION_NAME_COLLAPSE_CURRENT_SUBGRAPH "SimplifyGraph:collapse_current_subgraph"
#define ACTION_NAME_COMPLEMENT_CURRENT_GRAPH "SimplifyGraph:complement_current_subgraph"
#define ACTION_NAME_COMPLEMENT_EXPAND_CURRENT_GRAPH "SimplifyGraph:complement_expand_current_subgraph"

#define BUFFSIZE 512

#define CURRENT_SUBGRAPH_COLOR 0xb3ffb3
#define INVALID_GROUP_COLOR 0xb3b3ff

#define INVALID_GROUP_HIGHLIGHT -1

struct GraphHelpContext {
#if IDA_SDK_VERSION < 700
    TForm *tform;
    TCustomControl *tcontrol;
#else // #if IDA_SDK_VERSION < 700
    TWidget *tform;
    TWidget *tcontrol;
#endif // #if IDA_SDK_VERSION < 700
    mutable_graph_t *mgraph;
    int currNode;
};

//-------------------------------------------------------------------------
static const char PLUGINNAME[] = "SimplifyGraph";
static const char WANTED_NAME[] = "SimplifyGraph";
static const char PLUGIN_COMMENT[] = "FLARE subgraph grouping plugin";
static const char PLUGIN_HELP[] = "Attempts to create groups of nodes to simplify graphs during reversing.\n";
static const char POPUPPATH[] = "SimplifyGraph/";
static const char TITLE_SG_PANEL[] = "SimplifyGraph - Isolated subgraphs";
static const char STR_OUTWIN_TITLE[] = "Output window";
static const char MY_CONFIG_FILE[] = "SimplifyGraph.cfg";


//-------------------------------------------------------------------------
// globals suck
static Flare::FlareGraph g_lastFlowGraph;

//-------------------------------------------------------------------------
// Function prototypes
void idaapi runUniqueReachable(void);
void setupHooksAndActions(void);
bool collapsePostDomNodes(GraphHelpContext &ctx);
void printNodeInfo(const GraphHelpContext &ctx, int n, bool doSubgraphs = false) ;
bool handleSwitchStatementCollapse() ;
bool handleSwitchCaseCollapse(GraphHelpContext &ctx, Flare::FlareGraph &fg, int node, groups_crinfos_t &newGroups) ;
bool loadFlareGraphFromIda(GraphHelpContext &ctx, Flare::FlareGraph &fg, bool orig) ;
bool fillContext(GraphHelpContext &ctx) ;
bool createGroup(GraphHelpContext &ctx, Flare::FlareGraph &fg, const Flare::intset_t &group) ;
bool handleChangedView(qflow_chart_t *newfc) ;
bool doesGroupAlreadyExist(GraphHelpContext &ctx, Flare::FlareGraph &fg, const Flare::intset_t &group) ;
bool isValidNewGroup(mutable_graph_t *mgraph, const Flare::FlareGraph &fg, const Flare::intset_t &group) ;
bool updateContext(GraphHelpContext &ctx) ;
bool complementCurrentSubgraph() ;
bool complementCurrentGroup() ;
bool isGraphActive() ;

//-------------------------------------------------------------------------
// Config: These values can be overridden via IDA's .ini file parser

//g_minimumSubgraphNodeCount: subgraphs must be at least N nodes to justify creating a subgraph
unsigned int g_minimumSubgraphNodeCount = 3;

// g_maximumSubgraphNodePercentage: ignore subgraphs that make up more than N percent of nodes in the graph
unsigned int g_maximumSubgraphNodePercentage = 95;

// g_highLightNodeColor: RGB color value to use when highlighting subgraphs
unsigned int g_highLightNodeColor = CURRENT_SUBGRAPH_COLOR;


static const cfgopt_t g_opts[] = {
  cfgopt_t("MINIMUM_SUBGRAPH_NODE_COUNT", &g_minimumSubgraphNodeCount),
  cfgopt_t("MAXIMUM_SUBGRAPH_NODE_PERCENTAGE", &g_maximumSubgraphNodePercentage),
  cfgopt_t("SUBGRAPH_HIGHLIGHT_COLOR", &g_highLightNodeColor),
};

//-------------------------------------------------------------------------

// ColorCacheMap: keep a memory of the original line colors in an IDA view
// so we can restore when the plugin is closed/cleared.
class ColorCacheMap {
private:
    std::map<ea_t, bgcolor_t> cacheColorMap;

public:
    ea_t m_fstart;

    ColorCacheMap() : m_fstart(0) { }

    bool snapshot(ea_t _fstart) {
        m_fstart = _fstart;
        cacheColorMap.clear();
        func_t *pfn = get_func(m_fstart);
        if (!pfn) {
            msg("%s: Unable to get func_t for %08"EAFMTSTR"\n", PLUGINNAME, m_fstart);
            return false;
        }

        func_item_iterator_t fii;
        for ( bool ok = fii.set(pfn, m_fstart); ok; ok=fii.next_addr() ) {
            ea_t ea = fii.current();
            bgcolor_t col = get_item_color(ea);
            if (col != DEFCOLOR) {
                //DEBUGLOG("Caching color: %08"EAFMTSTR" -> %08x\n", ea, col);
                cacheColorMap[ea] = col;
            }
        }
        return true;
    }

    void clear() {
        cacheColorMap.clear();
    }

    // restore cached colors
    void restore() {
        func_t *pfn = get_func(m_fstart);
        if (!pfn) {
            msg("%s: Unable to get func_t for %08"EAFMTSTR"\n", PLUGINNAME, m_fstart);
            return;
        }
        func_item_iterator_t fii;
        for ( bool ok = fii.set(pfn, m_fstart); ok; ok=fii.next_addr() ) {
            ea_t ea = fii.current();
            auto it = cacheColorMap.find(ea);
            if (it == cacheColorMap.end()) {
                //DEBUGLOG("Restoring def color: %08"EAFMTSTR" -> %08x\n", ea, DEFCOLOR);
                set_item_color(ea, DEFCOLOR);

            } else {
                //DEBUGLOG("Restoring cached color: %08"EAFMTSTR" -> %08x\n", ea, it->second);
                set_item_color(ea, it->second);
            }
        }
        return;
    }
};

//-------------------------------------------------------------------------


// SubgraphChooser IDA chooser (wrapper < IDA700)
// Handles highlighting & creating isolated subgraphs
class SubgraphChooser CHOOSER_PARENT {

protected:
  static const int widths_[];
  static const char *const header_[];
 

private:
    static SubgraphChooser *m_singleton;
    GraphHelpContext m_ctx;
    Flare::FlareGraph m_fg;
    ColorCacheMap m_colorCache;
    Flare::subgraphVec_t m_subgraphs;
    intvec_t m_currentlySelected;
    size_t m_currentlyHighlightedGroup;
 
    //current function start address
    ea_t m_fstart;
    bool m_isActive;

    
#if IDA_SDK_VERSION < 700
    chooser_info_t m_chi;

    // static methods for chooser_info_t callbacks
    static uint32 idaapi s_sizer(void *obj) {
        return static_cast<SubgraphChooser *>(obj)->onGetSize();
    }

    static void idaapi s_getl(void *obj, uint32 n, char *const *arrptr) {
        static_cast<SubgraphChooser *>(obj)->onGetLine(n, arrptr);
    }

    static uint32 idaapi s_update(void *obj, uint32 n) {
        return static_cast<SubgraphChooser *>(obj)->onUpdate(n);
    }

    static uint32 idaapi s_del(void *obj, uint32 n) {
        return static_cast<SubgraphChooser *>(obj)->onDelete(n);
    }

    static void idaapi s_ins(void *obj) {
        static_cast<SubgraphChooser *>(obj)->onInsert();
    }

    static void idaapi s_enter(void *obj, uint32 n) {
        static_cast<SubgraphChooser *>(obj)->onEnter(n);
    }

    static void idaapi s_refresh(void *obj) {
        static_cast<SubgraphChooser *>(obj)->onRefresh();
    }

    static void idaapi s_initializer(void *obj) {
        static_cast<SubgraphChooser *>(obj)->onInit();
    }

    static void idaapi s_get_attrs(void *obj, uint32 n, chooser_item_attrs_t *attrs) {
        static_cast<SubgraphChooser *>(obj)->onGetAttrs(n, attrs);
    }
 
    static void idaapi s_destroyer(void *obj) {
        static_cast<SubgraphChooser *>(obj)->onDestroy();
    }

    static void idaapi s_edit(void *obj, uint32 n) {
        static_cast<SubgraphChooser *>(obj)->onRefresh();
        //static_cast<SubgraphChooser *>(obj)->onEditLine(n);
    }

    static void idaapi s_select(void *obj, const intvec_t &sel) {
        static_cast<SubgraphChooser *>(obj)->onSelect(sel);
    }

    void init_chi() ;
#endif // #if IDA_SDK_VERSION < 700

public:

    bool isActive() const { return m_isActive; }
    void setActive(bool st) { m_isActive = st; }


#if IDA_SDK_VERSION < 700

    SubgraphChooser() {
        init_chi();
        m_currentlyHighlightedGroup = INVALID_GROUP_HIGHLIGHT;
        m_isActive = false;
    }

#else  // #if IDA_SDK_VERSION < 700

    SubgraphChooser();

#endif // #if IDA_SDK_VERSION < 700

    virtual ~SubgraphChooser() {
    }

    bool clear() {
        DEBUGLOG("clear()\n");
        m_colorCache.restore();
        m_colorCache.clear();
        m_fg.clear();
        m_subgraphs.clear();
        m_currentlySelected.clear();
        m_currentlyHighlightedGroup = INVALID_GROUP_HIGHLIGHT;
        m_fstart = 0;
        return true;
    }

#if IDA_SDK_VERSION < 700
    uint32 onGetSize() {
        bool ret = updateContext(m_ctx);
        if (ret) {
            DEBUGLOG("SubgraphChooser::onGetSize: %d\n", m_subgraphs.size());
            return m_subgraphs.size();
        }
        DEBUGLOG("SubgraphChooser::onGetSize: not an active graph\n");
        clear();
        return 0;
    }

    void onGetLine(uint32 n, char *const *arrptr) {
        DEBUGLOG("SubgraphChooser::onGetLine: %d\n", n);
        if (n == 0) {
            qstrncpy(arrptr[0], "StartEA1", MAXSTR);
            qstrncpy(arrptr[1], "StartEA2", MAXSTR);
            qstrncpy(arrptr[2], "Count", MAXSTR);
            qstrncpy(arrptr[3], "Percent", MAXSTR);
        } else {
            // reminder: subtract 1 from n to account for title row
            --n;
            if (n >= m_subgraphs.size())
                return;
            Flare::subgraph_t &curr = m_subgraphs[n];
            qstring desc;
            Flare::area_t area1, area2;
            bool ret1 = g_lastFlowGraph.getNodeArea(curr.n1, area1);
            bool ret2 = g_lastFlowGraph.getNodeArea(curr.n2, area2);
            if (ret1 && ret2) {
                DEBUGLOG("Using format string 08"EAFMTSTR" for EAs\n");
                desc.sprnt("%08"EAFMTSTR, area1.start);
                qstrncpy(arrptr[0], desc.c_str(), MAXSTR);
                desc.sprnt("%08"EAFMTSTR, area2.start);
                qstrncpy(arrptr[1], desc.c_str(), MAXSTR);
            } 
            desc.sprnt("%d", curr.nodes.size());
            qstrncpy(arrptr[2], desc.c_str(), MAXSTR);
            if(m_fg.getNodeCount() == 0) {
                desc.sprnt("N/A");
            } else {
                desc.sprnt("%d", int((double(curr.nodes.size())/m_fg.getNodeCount())*100.0));
            }
            qstrncpy(arrptr[3], desc.c_str(), MAXSTR);
        }
    }

    uint32 onUpdate(uint32 n) {
        DEBUGLOG("SubgraphChooser::onUpdate %d\n", n);
        return n;
    }

    uint32 onDelete(uint32 n) {
        bool ret = updateContext(m_ctx);
        if (ret) {
            DEBUGLOG("SubgraphChooser::onDelete %d\n", n);
            commonOnDelete(n);
        } else {
            DEBUGLOG("SubgraphChooser::onDelete not active %d\n", n);
            clear();
        } 
        return n;
    }

    void onInsert() {
        bool ret = updateContext(m_ctx);
        if (!ret) {
            DEBUGLOG("SubgraphChooser::onInsert not active \n");
            clear();
            return;
        }
        DEBUGLOG("SubgraphChooser::onInsert: %d items\n", m_currentlySelected.size());
        for (auto it = m_currentlySelected.begin(); it != m_currentlySelected.end(); ++it) {
            // reminder: subtract 1 from row idx to account for title row
            commonOnInsert(*it -1);
        }
    }

    void onEnter(uint32 n) {
        bool ret = updateContext(m_ctx);
        if (!ret) {
            DEBUGLOG("SubgraphChooser::onEnter not active %d\n", n);
            clear();
            return;
        }
        DEBUGLOG("SubgraphChooser::onEnter %d\n", n);
        m_currentlySelected.clear();
        m_currentlySelected.push_back(n);
        // reminder: subtract 1 from n to account for title row
        commonOnEnter(n-1);
    }

    void onRefresh() {
        //if (isGraphActive()) {
            DEBUGLOG("SubgraphChooser::onRefresh\n");
            commonOnRefresh();
        //} else {
        //    DEBUGLOG("SubgraphChooser::onRefresh not active\n");
        //    clear();
        //} 
    }

    void onInit() {
        commonInit();
    }

    void onGetAttrs(uint32 n, chooser_item_attrs_t *attrs) {
        bool ret = updateContext(m_ctx);
        if (!ret) {
            DEBUGLOG("SubgraphChooser::onGetAttrs updateContext failed\n");
            return;
        }
        DEBUGLOG("SubgraphChooser::onGetAttrs\n");
        // reminder: subtract 1 from n to account for title row
        --n;
        if (n >= m_subgraphs.size())
            return;
        Flare::subgraph_t &curr = m_subgraphs[n];
        if (isValidNewGroup(m_ctx.mgraph, m_fg, curr.nodes)) {
            attrs->color = DEFCOLOR;
            attrs->flags = 0;
        } else {
            DEBUGLOG("Yes group %d already exists\n", n);
            attrs->color = INVALID_GROUP_COLOR;
            attrs->flags = (CHITEM_BOLD | CHITEM_GRAY);
        }
    }

    void onDestroy() {
        DEBUGLOG("SubgraphChooser::onDestroy\n");
        if (m_chi.popup_names != NULL) {
            qfree((void *)m_chi.popup_names);
        }
        clear();
        deleteSingleton();
    }

    void onEditLine(uint32 n) {
        DEBUGLOG("SubgraphChooser::onEditLine\n", n);
    }

    void onSelect(const intvec_t &sel) {
        DEBUGLOG("SubgraphChooser::onSelect %d items\n", sel.size());
        m_currentlySelected = sel;
    }

#else  // #if IDA_SDK_VERSION < 700

    virtual size_t idaapi get_count() const {
        if (isGraphActive()) {
            DEBUGLOG("SubgraphChooser::get_count(): %d\n", m_subgraphs.size());
            return m_subgraphs.size();
        }
        DEBUGLOG("SubgraphChooser::get_count(): not an active graph\n");
        //clear();
        return 0;
    }

    virtual bool init() {
        return commonInit();
    }

    virtual void idaapi closed() {
        DEBUGLOG("closed()\n");
        clear();
        m_isActive = false;
    }

    virtual void idaapi get_row( qstrvec_t *cols_, int *icon_, chooser_item_attrs_t *attrs, size_t n) const {
        if (n >= m_subgraphs.size())
            return;
        if (!isGraphActive()) {
            return;
        }

        const Flare::subgraph_t &curr = m_subgraphs[n];
        Flare::area_t area1, area2;
        qstrvec_t &cols = *cols_;
        DEBUGLOG("Size of cols: %d\n", cols.size());
        if (cols.size() < 4) {
            LOG("Bailing due to bad cols vector\n");
        }
        bool ret1 = g_lastFlowGraph.getNodeArea(curr.n1, area1);
        bool ret2 = g_lastFlowGraph.getNodeArea(curr.n2, area2);
        if (ret1 && ret2) {
            DEBUGLOG("Using format string 08"EAFMTSTR" for EAs\n");
            cols[0].sprnt("%08"EAFMTSTR, area1.start);
            cols[1].sprnt("%08"EAFMTSTR, area2.start);
        } 
        cols[2].sprnt("%d", curr.nodes.size());
        if(m_fg.getNodeCount() == 0) {
            cols[3].sprnt("N/A");
        } else {
            cols[3].sprnt("%d", int((double(curr.nodes.size())/m_fg.getNodeCount())*100.0));
        }

        if(!attrs) {
            LOG("Null attrs pointer\n");
        } else {
            DEBUGLOG("Maybe valid attrs pointer. size is %d (vs %d)\n", attrs->cb, sizeof(chooser_item_attrs_t));
            // arrrrgh! const! can't update any of my state
            mutable_graph_t *mgraph = get_viewer_graph(m_ctx.tcontrol);
            if (isValidNewGroup(mgraph, m_fg, curr.nodes)) {
                DEBUGLOG("No group %d already exists\n", n);
                attrs->color = DEFCOLOR;
                attrs->flags = 0;
            } else {
                DEBUGLOG("Yes group %d already exists\n", n);
                attrs->color = INVALID_GROUP_COLOR;
                attrs->flags = (CHITEM_BOLD | CHITEM_GRAY);
            }
        }
    }

    virtual cbret_t idaapi enter(size_t n) { 
        DEBUGLOG("enter: %d\n", n);
        if (isGraphActive()) {
            commonOnEnter(n);
        } else {
            clear();
        }
        return cbret_t(n, ALL_CHANGED); 
    }

    virtual cbret_t idaapi ins(ssize_t n) { 
        DEBUGLOG("ins: %d\n", n);
        if (isGraphActive()) {
            commonOnInsert(n);
        } else {
            clear();
        }
        return cbret_t(n, ALL_CHANGED); 
    }

    virtual cbret_t idaapi del(size_t n) {
        DEBUGLOG("del: %d\n", n);
        if (isGraphActive()) {
            commonOnDelete(n);
        } else {
            clear();
        }
        return cbret_t(n, ALL_CHANGED); 
    }

    virtual cbret_t idaapi refresh(ssize_t n) {
        DEBUGLOG("refresh: %d\n", n);
        if (isGraphActive()) {
            commonOnRefresh();
        } else {
            clear();
        }
        return cbret_t(n, ALL_CHANGED);
    }

#endif // #if IDA_SDK_VERSION < 700

    bool commonInit() {
        DEBUGLOG("commonInit\n");
        if (g_lastFlowGraph.getNodeCount() == 0) {
            msg("%s: Empty flow chart\n", PLUGINNAME);
            return false;
        }
        m_fstart = g_lastFlowGraph.getFunctionEa();
        loadSubgraphs();
        return true;
    }

    bool commonOnDelete(size_t n) {
        m_colorCache.restore();
        m_currentlyHighlightedGroup = INVALID_GROUP_HIGHLIGHT;
        return true;
    }

    bool commonOnEnter(size_t n) {
        DEBUGLOG("SubgraphChooser::commonOnEnter %d\n", n);
        // adjust for column name row
        if (n >= m_subgraphs.size())
            return false;
        Flare::subgraph_t &curr = m_subgraphs[n];
        m_currentlyHighlightedGroup = n;
        //reset colors
        func_item_iterator_t fii;
        m_colorCache.restore();
        bool ret = m_colorCache.snapshot(m_fstart);
        if (!ret) { 
            msg("%s: Failed to cache colors\n", PLUGINNAME);
            return false;
        }

        Flare::area_t area1;
        for (auto it = curr.nodes.begin(); it != curr.nodes.end(); ++it) {
            bool ret1 =  g_lastFlowGraph.getNodeArea(*it, area1);
            if (ret1) {
                DEBUGLOG("Applying color to node %d: %08"EAFMTSTR" - %08"EAFMTSTR"\n", *it, area1.start, area1.end);
                colorBlock(area1, g_highLightNodeColor);
            } else {
                msg("%s: Unable to get node %d area_t\n", PLUGINNAME, *it);
            }

        }
        Flare::fea_t jumpea = g_lastFlowGraph.getNodeAreaStart(curr.n1);
        staticClearRefreshChooser(false);
        jumpto(jumpea);
        msg("%s: Highlighting isolated at subgraph %08"EAFMTSTR"\n", PLUGINNAME, jumpea);
        return true;
    }

    bool commonOnInsert(ssize_t idx) {
        DEBUGLOG("%s: Creating group %d of %d\n", PLUGINNAME, idx, m_subgraphs.size());
        bool ret = createGroup(m_ctx, m_fg, m_subgraphs[idx].nodes);
        if (ret) {
            DEBUGLOG("Created new group\n");
        } else {
            msg("%s: Failed to create group\n", PLUGINNAME);
        }
        m_colorCache.restore();
        return true;
    }

    bool commonOnRefresh() {
        // check if we're in a new function or not
        //if (!isGraphActive()) {
        //    clear();
        //    return false;
        //}
        if (g_lastFlowGraph.getNodeCount() == 0) {
            msg("%s: Emtpy flow chart\n", PLUGINNAME);
            return false;
        }
        if (m_fstart == g_lastFlowGraph.getFunctionEa() ) {
            // pass
            DEBUGLOG("Skipping redundant subgraph load\n");
        } else {
            DEBUGLOG("Loading m_subgraphs now\n");
            m_fstart = g_lastFlowGraph.getFunctionEa() ;
            loadSubgraphs();
        }
        return true;
    }

    bool loadSubgraphs() {
        DEBUGLOG("SubgraphChooser::loadSubgraphs\n");
        bool ret = fillContext(m_ctx);
        if (!ret) {
            msg("%s: Failed to load graph context\n", PLUGINNAME);
            return false;
        }
        m_fg.clear();
        // load the original function graph
        ret = loadFlareGraphFromIda(m_ctx, m_fg, true);
        if (!ret) {
            msg("%s: Failed to load FlareGraph from IDA\n", PLUGINNAME);
            return false;
        }
        cacheCurrentFunctionColors();
        ret = m_fg.findSimpleSubGraphs(m_subgraphs, g_minimumSubgraphNodeCount, g_maximumSubgraphNodePercentage);
        if (!ret) {
            msg("%s: findSimpleSubGraphs failed :(\n", PLUGINNAME);
            return false;
        }
        DEBUGLOG("%s: Found %d interesting subgraphs\n", PLUGINNAME, m_subgraphs.size());
        DEBUGEXEC({
            for (auto it = m_subgraphs.begin(); it != m_subgraphs.end(); ++it) {
                DEBUGLOG("Got interesting subgraph %d nodes\n", it->nodes.size());
                for (auto git = it->nodes.begin(); git != it->nodes.end(); ++git) {
                    Flare::area_t ar1;
                    g_lastFlowGraph.getNodeArea(*git, ar1);
                    DEBUGLOG("  %d: %08"EAFMTSTR" - %08"EAFMTSTR"\n", *git, ar1.start, ar1.end);
                }
            }
        });
        return true;
    }

    void colorBlock(const Flare::area_t &area, bgcolor_t color) {
        ea_t curr = area.start;
        while (curr != area.end) {
            set_item_color(curr, color);
            curr = next_head(curr, curr+0x40);
        }
    }

    void deleteSingleton() {
        if (m_singleton != NULL) {
            DEBUGLOG("Deleting m_singleton\n");
            delete m_singleton;
            m_singleton = NULL;
        }
    }

    bool cacheCurrentFunctionColors() {
        bool ret = true;
        if (g_lastFlowGraph.getNodeCount() == 0) {
            msg("%s: Empty flow chart\n", PLUGINNAME);
            return false;
        }
        DEBUGLOG("Examining flowchart at %08"EAFMTSTR"\n", m_fstart);
        return m_colorCache.snapshot(m_fstart);
    }

    bool createNodeOfCurrentHighlight() {
        if (m_currentlyHighlightedGroup == INVALID_GROUP_HIGHLIGHT) {
            return false;
        }
        msg("Creating group: %d of %d\n", m_currentlyHighlightedGroup, m_subgraphs.size());
        bool ret = createGroup(m_ctx, m_fg, m_subgraphs[m_currentlyHighlightedGroup].nodes);
        if (ret) {
            DEBUGLOG("Created new group\n");
        } else {
            msg("%s: Failed to create group\n", PLUGINNAME);
        }
        m_colorCache.restore();
        m_currentlyHighlightedGroup = INVALID_GROUP_HIGHLIGHT;
        staticClearRefreshChooser(false);
        return true;
    }

    bool isNodeHighlighted(int node) {
        DEBUGLOG("isNodeHighlighted: %d: ");
        if (m_currentlyHighlightedGroup == INVALID_GROUP_HIGHLIGHT) {
            DEBUGLOG("No\n");
            return false;
        }
        if (m_currentlyHighlightedGroup >= m_subgraphs.size()) {
            LOG("Bad node to isNodeHighlighted: (curr highligh: %p) vs (subgraphs count: %p)\n", m_currentlyHighlightedGroup, m_subgraphs.size());
            return false;
        }
        Flare::subgraph_t &curr = m_subgraphs[m_currentlyHighlightedGroup];
        if (curr.nodes.find(node) == curr.nodes.end()) {
            DEBUGLOG("No\n");
            return false;
        }
        DEBUGLOG("yes\n");
        return true;
    }

    void onShow() {
        DEBUGLOG("Setting dock position\n");
        set_dock_pos(TITLE_SG_PANEL, STR_OUTWIN_TITLE, DP_RIGHT);
    }

    // start of the static convenience accessors

    static bool show() {
        DEBUGLOG("SubgraphChooser::show\n");
        if (m_singleton == NULL) {
            DEBUGLOG("Creating new chooser\n");
            m_singleton = new SubgraphChooser();
        }

#if IDA_SDK_VERSION < 700

        choose3(&m_singleton->m_chi);

#else  // #if IDA_SDK_VERSION < 700

        m_singleton->setActive(true);
        m_singleton->choose();

#endif // #if IDA_SDK_VERSION < 700

        m_singleton->onShow();
        return true;
    }

    //TForm* getTForm() {
    //    return m_ctx.tform;
    //}

    //---------------------------------------------------------------------
    // start of static helpers

    // returns true if there is an active chooser
    static bool staticIsActive() {
        if(m_singleton == NULL) {
            return false;
        }
        DEBUGLOG("chooser active\n");
        return m_singleton->isActive();
    }

    static void staticDeleteChooser() {
        if (m_singleton != NULL) {
            m_singleton->deleteSingleton();
        }
    }

    static void staticClearRefreshChooser(bool doclear = true) {
        if (m_singleton != NULL) {
            if(doclear) {
                m_singleton->clear();
            }
            refresh_chooser(TITLE_SG_PANEL);
        }
    }

    static bool staticCreateNodeOfCurrentHighlight() {
        if (m_singleton == NULL) {
            return false;
        }
        return m_singleton->createNodeOfCurrentHighlight();
    }


    static bool isNodeActivelyHighlighted(int node) {
        if (m_singleton == NULL) {
            return false;
        }
        return m_singleton->isNodeHighlighted(node);
    }

    static bool staticCloseChooser() {
        if (m_singleton == NULL) {
            return true;
        }
        // not great, but at least it won't crash in ida 695
        // issue is that for ida 7.0 & newer, i can check the current active window on chooser actions
        // for older ida, the active tform IS the chooser, so i can't tell if 
        close_chooser(TITLE_SG_PANEL);
        return true;
    }


#if IDA_SDK_VERSION < 700
    static bool staticHandleTFormChanged(TForm *tform) {
        if (m_singleton == NULL) {
            return true;
        }
        // not great, but at least it won't crash in ida 695
        // issue is that for ida 7.0 & newer, i can check the current active window on chooser actions
        // for older ida, the active tform IS the chooser, so i can't tell if 
        close_chooser(TITLE_SG_PANEL);
        //if (m_singleton->getTForm() == tform) {
        //    LOG("singleton's tform changed. clear it out\n");
        //    staticClearRefreshChooser(true);
        //} else {
        //    LOG("singleton's tform not equal.\n");
        //}
        return true;
    }
#endif // #if IDA_SDK_VERSION < 700


};

const int SubgraphChooser::widths_[] = { 
    8, 
    8, 
    4, 
    4 
};

const char *const SubgraphChooser::header_[] = { 
    "StartEA1", 
    "StartEA2", 
    "Count",
    "Percent" 
};

#if IDA_SDK_VERSION < 700

void SubgraphChooser::init_chi() {
    // moved down here so that qnumber(widths_) works
    memset(&m_chi, 0, sizeof(m_chi));
    m_chi.cb            = sizeof(m_chi);
    m_chi.flags         = CH_ATTRS;
    m_chi.width         = -1;
    m_chi.height        = -1;
    m_chi.title         = TITLE_SG_PANEL;
    m_chi.obj           = this;
    m_chi.columns       = qnumber(widths_);
    m_chi.widths        = widths_;
    m_chi.icon          = -1;
    m_chi.deflt         = 0;
    m_chi.sizer         = s_sizer;
    m_chi.getl          = s_getl;
    m_chi.ins           = s_ins;
    m_chi.del           = s_del;
    m_chi.enter         = s_enter;
    m_chi.update        = s_update;
    m_chi.destroyer     = s_destroyer;
    m_chi.refresh       = s_refresh;
    m_chi.select        = s_select;
    m_chi.edit          = s_edit;
    m_chi.initializer   = s_initializer;
    m_chi.get_attrs     = s_get_attrs;

    m_chi.popup_names = (const char **)qalloc(sizeof(char *) * 5);
    *(((char **)m_chi.popup_names)+0) = "Create group"; // Insert
    *(((char **)m_chi.popup_names)+1) = "Clear highlights"; // Delete
    *(((char **)m_chi.popup_names)+2) = "Refresh groups"; // Edit
    *(((char **)m_chi.popup_names)+3) = NULL; // Refresh
    *(((char **)m_chi.popup_names)+4) = NULL; // Copy
}

#else // #if IDA_SDK_VERSION < 700

// moved down here so that qnumber(widths_) works
SubgraphChooser::SubgraphChooser() : chooser_t( 
            CH_KEEP | CH_CAN_INS | CH_CAN_DEL | CH_CAN_REFRESH, 
            qnumber(widths_), 
            widths_, 
            header_, 
            TITLE_SG_PANEL) {

        popup_names[ POPUP_INS ] = "Create group"; // Insert
        popup_names[ POPUP_DEL ] = "Clear highlights"; // Delete
        popup_names[ POPUP_REFRESH ]  = "Refresh groups"; // Refresh

        m_currentlyHighlightedGroup = INVALID_GROUP_HIGHLIGHT;
        m_isActive = false;
}

#endif // #if IDA_SDK_VERSION < 700

SubgraphChooser *SubgraphChooser::m_singleton = NULL;


// Fills the given context structure based on the current form/view
bool updateContext(GraphHelpContext &ctx) {
    ctx.mgraph = get_viewer_graph(ctx.tcontrol);
    if (ctx.mgraph == NULL) {
        DEBUGLOG("%s: Failed to get mutable_graph\n", PLUGINNAME);
        return false;
    }
    ctx.currNode = viewer_get_curnode(ctx.tcontrol);
    return true; 
}

// we need to disable activity if there is not an active
// disassembly graph view. returns true if the active view
// is a disassembly in graph mode, otherwise false
bool isGraphActive() {
    GraphHelpContext ctx = { 0 } ;

#if IDA_SDK_VERSION < 700
    ctx.tform = get_current_tform();
    if (ctx.tform == NULL) {
        msg("%s: Failed to get current TForm\n", PLUGINNAME);
        return false;
    }
    if (get_tform_type(ctx.tform) != BWN_DISASM) {
        DEBUGLOG("%s: Oops. Not a disssembly tform: %d\n", PLUGINNAME, get_tform_type(ctx.tform));
        return false;
    }
    ctx.tcontrol = get_tform_idaview(ctx.tform);

#else // #if IDA_SDK_VERSION < 700

    ctx.tcontrol = get_current_viewer();
    if (ctx.tcontrol == NULL) {
        DEBUGLOG("%s: Failed to get TCustomControl\n", PLUGINNAME);
        return false;
    }
    if (get_widget_type(ctx.tcontrol) != BWN_DISASM) {
        DEBUGLOG("%s: Oops. Not a disssembly tform\n", PLUGINNAME);
        return false;
    }

#endif  // #if IDA_SDK_VERSION < 700

    if (get_view_renderer_type(ctx.tcontrol) != TCCRT_GRAPH) {
        DEBUGLOG("%s: Not currently in graph mode. Bailing out.\n", PLUGINNAME);
        return false;
    }
    return true;
}

// Fills the given context structure based on the current form/view
bool fillContext(GraphHelpContext &ctx) {
    ctx.tform = NULL;
    ctx.tcontrol = NULL;
    ctx.mgraph = NULL;
    ctx.currNode = -1;

#if IDA_SDK_VERSION < 700

    ctx.tform = get_current_tform();
    if (ctx.tform == NULL) {
        msg("%s: Failed to get current TForm\n", PLUGINNAME);
        return false;
    }
    if (get_tform_type(ctx.tform) != BWN_DISASM) {
        msg("%s: Oops. Not a disssembly tform b: %d\n", PLUGINNAME, get_tform_type(ctx.tform));
        return false;
    }
    ctx.tcontrol = get_tform_idaview(ctx.tform);

#else // #if IDA_SDK_VERSION < 700

    ctx.tcontrol = get_current_viewer();
    if (ctx.tcontrol == NULL) {
        msg("%s: Failed to get TCustomControl\n", PLUGINNAME);
        return false;
    }
    if (get_widget_type(ctx.tcontrol) != BWN_DISASM) {
        msg("%s: Oops. Not a disssembly tform b:\n", PLUGINNAME);
        return false;
    }

#endif  // #if IDA_SDK_VERSION < 700

    if (get_view_renderer_type(ctx.tcontrol) != TCCRT_GRAPH) {
        DEBUGLOG("%s: Not currently in graph mode. Bailing out.\n", PLUGINNAME);
        return false;
    }

    ctx.mgraph = get_viewer_graph(ctx.tcontrol);
    if (ctx.mgraph == NULL) {
        msg("%s: Failed to get mutable_graph\n", PLUGINNAME);
        return false;
    } else {
        DEBUGLOG("%s: got mutable_graph: %d items\n", PLUGINNAME, ctx.mgraph->size());
    }
    ctx.currNode = viewer_get_curnode(ctx.tcontrol);
    return true;
}

// returns true if it looks like the current node is a switch statement
//  i.e. it has more than 2 visible successor nodes
static bool isSwitchStatementNode() {
    GraphHelpContext ctx;
    bool ret = fillContext(ctx);
    if (!ret) {
        return ret;
    }
    if(ctx.currNode < 0) {
        return false;
    }
    int visibleCount = 0;
    for (int i = 0; i < ctx.mgraph->nsucc(ctx.currNode); ++i) {
        int succ = ctx.mgraph->succ(ctx.currNode, i);
        if (ctx.mgraph->is_visible_node(succ)) {
            ++visibleCount;
        }
    }
    return visibleCount > 2;
}

// returns true if the current node is a subgroup node, i.e. it belongs to a group
static bool isSubgroupNode() {
    GraphHelpContext ctx;
    bool ret = fillContext(ctx);
    if (!ret) {
        return false;
    }
    if(ctx.currNode < 0) {
        return false;
    }
    return ctx.mgraph->is_subgraph_node(ctx.currNode);
}

// returns true if the current node is a group node, i.e. it represents a collapsed group of nodes
static bool isGroupNode() {
    GraphHelpContext ctx;
    bool ret = fillContext(ctx);
    if (!ret) {
        return false;
    }
    if(ctx.currNode < 0) {
        return false;
    }
    return ctx.mgraph->is_group_node(ctx.currNode);
}


// returns true if all nodes of potential new group are not currently in another group
static bool isValidNewGroup(mutable_graph_t *mgraph, const Flare::FlareGraph &fg, const Flare::intset_t &group) {
    for (auto it = group.begin(); it != group.end(); ++it) {
        if (mgraph->is_subgraph_node(*it)) {
            int ngroup = mgraph->get_node_group(*it);
            DEBUGLOG("isValidNewGroup Node %d is in group %d\n", *it, ngroup);
            return false;
        }
    }
    // all nodes belong to the same group, return true
    DEBUGLOG("isValidNewGroup all nodes are free of groups\n");
    return true;
}

// checks to see if all of the nodes in group already exist as a group to avoid needlessly recreating them
static bool doesGroupAlreadyExist(GraphHelpContext &ctx, Flare::FlareGraph &fg, const Flare::intset_t &group) {
    int knownGroup = -1;
    int totalCount = 0;
    int ungroupCount = 0;
    for (auto it = group.begin(); it != group.end(); ++it) {
        ++totalCount;
        if (*it < 0) {
            ++ungroupCount;
        }
        if (knownGroup < 0) {
            // first one, so just store the value
            knownGroup = ctx.mgraph->get_node_group(*it);
            DEBUGLOG("doesGroupAlreadyExist:  %d -> %d\n", *it, knownGroup);
        } else {
           //DEBUGLOG("doesGroupAlreadyExist:  %d -> %d\n", *it, ctx.mgraph->get_node_group(*it));
           if (ctx.mgraph->get_node_group(*it) != knownGroup) {
               // two nodes belong to different groups, return false
               DEBUGLOG("doesGroupAlreadyExist Found mismatch\n");
               return false;
           }
        }
    }
    // all nodes belong to the same group, return true
    DEBUGLOG("doesGroupAlreadyExist Found MATCHING GROUP!!\n");
    return true;
}

// Helper to create a single new group and then center the view
static bool createGroup(GraphHelpContext &ctx, Flare::FlareGraph &fg, const Flare::intset_t &group) {
    //first translate to IDA types
    group_crinfo_t newGroup;
    DEBUGLOG("%s: Creating new group: %d nodes\n\t", PLUGINNAME, group.size());
    updateContext(ctx);

    if (doesGroupAlreadyExist(ctx, fg, group)) {
        msg("%s: Skipping unnecessary group creation\n", PLUGINNAME);
        return true;
    }
    if (!isValidNewGroup(ctx.mgraph, fg, group)) {
        msg("%s: Skipping group creation. Conflicting existing node groups\n", PLUGINNAME);
        return true;
    }

    DEBUGLOG("Ok, actually making the group\n");

    for (auto it = group.begin(); it != group.end(); ++it) {
        newGroup.nodes.push_back(*it);
    }
    DEBUGLOG("%s: Creating new group: %d of %d total nodes\n", PLUGINNAME, newGroup.nodes.size(), fg.getNodeCount()); 

    groups_crinfos_t newGroups;

#if IDA_SDK_VERSION < 700

    bool hasUserInput = askqstr(&(newGroup.text), "New node text");

#else // #if IDA_SDK_VERSION < 700

    bool hasUserInput = ask_str(&(newGroup.text), HIST_CMT, "New node text");

#endif // #if IDA_SDK_VERSION < 700

    if (!hasUserInput) {
        msg("%s: Bailing due to user not entering new node text\n", PLUGINNAME);
        return false;
    }
    newGroups.push_back(newGroup);
    intvec_t out_group_nodes;
    bool ret = viewer_create_groups(ctx.tcontrol, &out_group_nodes, newGroups);
    if(ret) {
        DEBUGLOG("New group success!\n");
        if (out_group_nodes.size() == 1) {
            node_info_t ninfo;
            memset(&ninfo, 0, sizeof(node_info_t));
            ret = viewer_get_node_info(ctx.tcontrol, &ninfo, out_group_nodes[0]);
            if (ret) {
                // viewer_center_on -> causes IDA to crash :(
                //viewer_center_on(newcontrol, out_group_nodes[0]);
                jumpto(ninfo.ea, 0, UIJMP_DONTPUSH | UIJMP_ACTIVATE | UIJMP_IDAVIEW);
            } else {
                msg("%s: Failed at viewer_get_node_info\n", PLUGINNAME);
                return false;
            }
            DEBUGEXEC(ctx.currNode = out_group_nodes[0];);
            DEBUGEXEC(printNodeInfo(ctx, ctx.currNode); );
            msg("%s: Created new subgroup at %08"EAFMTSTR"\n", PLUGINNAME, ninfo.ea);
        } else {
            msg("%s: Can't center on result node. Result size is %d\n", PLUGINNAME, out_group_nodes.size());
            return false;
        }

    } else {
        msg("%s: New group failed :( !\n", PLUGINNAME);
        return false;
    }

    return true;
}

// Detects if g_lastFlowGraph needs to be updated, and if so
// stores the current flowgraph.
// I used to keep a dangling pointer to IDA's qflow_chart_t, but 
// it turns out THAT IS NOT A GOOD IDEA!
bool loadCurrentFlowChart(qflow_chart_t *fc) {
    if ((!fc) || (fc->size() == 0)) {
        msg("%s: Bailing out of loadCurrentFlowChart due to empty flowchart. SHOULD NOT HAPPEN (I think)\n", PLUGINNAME);
        g_lastFlowGraph.clear();
    }
    if ((g_lastFlowGraph.getFunctionEa() == fc->blocks[0].STARTEA) && (g_lastFlowGraph.getNodeCount() == fc->size())) {
        // assuming they're identical. nothing to do
        return true;
    }
    DEBUGLOG("Loading updated flowchart: %08"EAFMTSTR"\n", fc->blocks[0].STARTEA);
    g_lastFlowGraph.clear();

#if IDA_SDK_VERSION < 700

    g_lastFlowGraph.setFilePath(database_idb);

#else  // #if IDA_SDK_VERSION < 700

    g_lastFlowGraph.setFilePath(get_path(PATH_TYPE_IDB));

#endif // #if IDA_SDK_VERSION < 700

    g_lastFlowGraph.setFunctionEa(fc->blocks[0].STARTEA);
    for (int i = 0; i < fc->size(); ++i) {
        g_lastFlowGraph.addNode(i);
        for (int j = 0; j < fc->nsucc(i); j++) {
            int succ = fc->succ(i, j);
            //no penalty for re-adding duplicate nodes. Same names assumed
            // to be the same node
            g_lastFlowGraph.addNode(succ);
            g_lastFlowGraph.addEdge(i, succ);
        }
        g_lastFlowGraph.setNodeArea(i, fc->blocks[i].STARTEA, fc->blocks[i].ENDEA);
    }
    DEBUGEXEC({
        DEBUGLOG("Just loaded FlareGraph\n");
        g_lastFlowGraph.log();
    });
    return true;
}


// if orig is true, only looks at nodes that are not subgraphs (i.e. the original nodes)
// if orig is false, only looks at currently displayed nodes
bool loadFlareGraphFromIda(GraphHelpContext &ctx, Flare::FlareGraph &fg, bool orig) {
    //first add all nodes & edges
    bool ret;
    // database_idb -> from idasdk. full path of IDB file

#if IDA_SDK_VERSION < 700

    g_lastFlowGraph.setFilePath(database_idb);

#else  // #if IDA_SDK_VERSION < 700

    fg.setFilePath(get_path(PATH_TYPE_IDB));

#endif // #if IDA_SDK_VERSION < 700

    int sentryCount = 0;

    if (orig) {
        // load the original flowgraph, stored in g_lastFlowGraph
        fg.load(g_lastFlowGraph);
    } else {
        fg.clear();
        // load the current viewable graph
        for (int i = 0; i < ctx.mgraph->size(); ++i) {
            if (ctx.mgraph->is_visible_node(i)) {
                fg.addNode(i);
                for (int j = 0; j < ctx.mgraph->nsucc(i); j++) {
                    int succ = ctx.mgraph->succ(i, j);
                    if (ctx.mgraph->is_visible_node(succ)) {
                        //no penalty for re-adding duplicate nodes. Same names assumed
                        // to be the same node
                        int newNode = fg.addNode(succ);
                        fg.addEdge(i, succ);
                    }
                }
            }
        }
    }
    // now add properties
    screen_graph_selection_t graphSel;
    ret = viewer_get_selection(ctx.tcontrol, &graphSel);
    if (!ret) {
        msg("%s: Failed to get graph selection\n", PLUGINNAME);
        return false;
    }
    if (graphSel.size() == 0) {
        DEBUGLOG("%s: No graph nodes selected to be root or sentries\n", PLUGINNAME);
    } else {
        int count = 0;
        for (screen_graph_selection_t::const_iterator p = graphSel.begin(); p != graphSel.end(); ++p) {
            if (p->is_node) {
                if (count == 0) {
                    fg.addNodeProp(p->node, Flare::NodeProperty::Root);
                    DEBUGLOG("Setting root node: %d\n", p->node);
                } else {
                    fg.addNodeProp(p->node, Flare::NodeProperty::Sentry);
                    DEBUGLOG("Adding sentry node: %d\n", p->node);
                    ++sentryCount;
                }
                ++count;
            }
        }
    }
    fg.setFunctionEa(ctx.mgraph->gid);
    DEBUGLOG("%s: Loaded graph at %08"EAFMTSTR": %d nodes, %d edges, %d sentries\n", 
            PLUGINNAME, ctx.mgraph->gid, fg.getNodeCount(), fg.getEdgeCount(), sentryCount
    );
    return true;
}


//-------------------------------------------------------------------------
// Action Handlers

struct CreateSubgraphActionHandler : public action_handler_t {
    virtual int idaapi activate(action_activation_ctx_t *) {
        DEBUGLOG("CreateSubgraphActionHandler called\n");
        runUniqueReachable();
        return true;
    }

    virtual action_state_t idaapi update(action_update_ctx_t *) {
        return AST_ENABLE_ALWAYS;
    }
};


// dot file writing (with custom property setting/abuse)
// just for debugging/testing initially
struct CreateDotFileActionHandler : public action_handler_t {
    virtual int idaapi activate(action_activation_ctx_t *) {
        DEBUGLOG("CreateDotFileActionHandler called\n");
        GraphHelpContext ctx;
        bool ret = fillContext(ctx);
        if (!ret) {
            return false;
        }
        Flare::FlareGraph fg;
        ret = loadFlareGraphFromIda(ctx, fg, false);
        if (!ret) {
            msg("%s: Failed to load FlareGraph from IDA\n", PLUGINNAME);
            return false;
        }

#if IDA_SDK_VERSION < 700

        char* outfile = askfile2_c(true, NULL, "*.dot", "Enter file to save GraphViz file to");

#else  // #if IDA_SDK_VERSION < 700

        char* outfile = ask_file(true, NULL, "*.dot", "Enter file to save GraphViz file to");

#endif // #if IDA_SDK_VERSION < 700

        if (!outfile) {
            msg("%s: Skipping output GraphViz due to user pressing escape", PLUGINNAME);
        } else {
            //orange TODO: does outfile need to be free'd?
            std::ostringstream ostringstr;
            fg.storeGraphViz(ostringstr);
            FILE *outf = qfopen(outfile, "w");
            std::string outs = ostringstr.str();
            qfwrite(outf, outs.c_str(), outs.size());
            qfclose(outf);
        }
        return true;
    }

    virtual action_state_t idaapi update(action_update_ctx_t *) {
        return AST_ENABLE_ALWAYS;
    }
};

struct PostDomSubgraphActionHandler : public action_handler_t {
    virtual int idaapi activate(action_activation_ctx_t *) {
        DEBUGLOG("PostDomSubgraphActionHandler called\n");
        
        GraphHelpContext ctx;
        bool ret = fillContext(ctx);
        if (!ret) {
            return false;
        }
        if(ctx.currNode < 0) {
            return false;
        }
        return collapsePostDomNodes(ctx);
    }

    virtual action_state_t idaapi update(action_update_ctx_t *) {
        return AST_ENABLE_ALWAYS;
    }
};

struct ComplementGraphActionHandler : public action_handler_t {
    virtual int idaapi activate(action_activation_ctx_t *) {
        DEBUGLOG("ComplementGraphActionHandler called\n");
        return complementCurrentSubgraph();
    }

    virtual action_state_t idaapi update(action_update_ctx_t *) {
        return AST_ENABLE_ALWAYS;
    }
};

struct ComplementExpandGraphActionHandler : public action_handler_t {
    virtual int idaapi activate(action_activation_ctx_t *) {
        DEBUGLOG("ComplementExpandGraphActionHandler called\n");
        return complementCurrentGroup();
    }

    virtual action_state_t idaapi update(action_update_ctx_t *) {
        return AST_ENABLE_ALWAYS;
    }
};

struct SwitchCaseActionHandler : public action_handler_t {
    virtual int idaapi activate(action_activation_ctx_t *) {
        DEBUGLOG("SwitchCaseActionHandler called\n");
        return handleSwitchStatementCollapse();
    }

    virtual action_state_t idaapi update(action_update_ctx_t *) {
        return AST_ENABLE_ALWAYS;
    }
};

struct DiscoverIsolatedSubgraphsHandler : public action_handler_t {
    virtual int idaapi activate(action_activation_ctx_t *) {
        DEBUGLOG("DiscoverIsolatedSubgraphsHandler called\n");
        return SubgraphChooser::show(); 
    }

    virtual action_state_t idaapi update(action_update_ctx_t *) {
        return AST_ENABLE_ALWAYS;
    }
};

struct CollapseCurrentSubgraphsHandler : public action_handler_t {
    virtual int idaapi activate(action_activation_ctx_t *) {
        DEBUGLOG("CollapseCurrentSubgraphsHandler called\n");
        SubgraphChooser::staticCreateNodeOfCurrentHighlight();
        return 0;
    }

    virtual action_state_t idaapi update(action_update_ctx_t *) {
        return AST_ENABLE_ALWAYS;
    }
};



static CreateSubgraphActionHandler g_sgah;
static CreateDotFileActionHandler g_dotah;
static PostDomSubgraphActionHandler g_pdah;
static SwitchCaseActionHandler g_switchah;
static DiscoverIsolatedSubgraphsHandler g_discoversubgraphah;
static CollapseCurrentSubgraphsHandler g_collasepsubgraphah;
static ComplementGraphActionHandler g_complementsubgraphah;
static ComplementExpandGraphActionHandler g_complementexpandsubgraphah;

//-------------------------------------------------------------------------
static const action_desc_t actions[] = {
    ACTION_DESC_LITERAL(ACTION_NAME_COLLAPSE_UNIQUE_REACH, "Create UR group", &g_sgah, NULL, NULL, -1), 
    ACTION_DESC_LITERAL(ACTION_NAME_COLLAPSE_SWITCH_CASES, "Create switch case groups", &g_switchah, NULL, NULL, -1),
    ACTION_DESC_LITERAL(ACTION_NAME_DISCOVER_ISOLATED_SUBGRAPHS, "Discover isolated subgraphs", &g_discoversubgraphah, NULL, NULL, -1),
    ACTION_DESC_LITERAL(ACTION_NAME_COLLAPSE_CURRENT_SUBGRAPH, "Create isolated subgraph", &g_collasepsubgraphah, NULL, NULL, -1),
    ACTION_DESC_LITERAL(ACTION_NAME_COMPLEMENT_CURRENT_GRAPH, "Complement group", &g_complementsubgraphah, NULL, NULL, -1),
    ACTION_DESC_LITERAL(ACTION_NAME_COMPLEMENT_EXPAND_CURRENT_GRAPH, "Complement & expand group", &g_complementexpandsubgraphah, NULL, NULL, -1),

#ifdef _DEBUG

    ACTION_DESC_LITERAL(ACTION_NAME_COLLAPSE_POST_DOM, "Create PD subgraph", &g_pdah, NULL, NULL, -1), 
    ACTION_DESC_LITERAL(ACTION_NAME_CREATE_DOTFILE, "Export dot file", &g_dotah, NULL, NULL, -1),

#endif // #ifdef _DEBUG

};
//
//-------------------------------------------------------------------------

// attempt to expand the specified group upon timer callback
// XXX TODO orange
// SUPER HACKY ATTEMPT TO GET AROUND UNKNOWN WAY TO DO GRAPH MODIFICATIONS
// DURING GRAPH ANIMATION, OR QUEUE WHEN ANIMATION COMPLETES. I TRIED SO MANY
// OTHER THINGS, AND NOTHING ELSE WORKED. DON'T BLAME ME IF THIS FAILS.
int idaapi complementCallback(void *ud) {
    int nodeToExpand = (int)ud;
    intvec_t toExpand;

    GraphHelpContext ctx;
    DEBUGLOG("complementCallback %d\n", nodeToExpand);
    bool ret = fillContext(ctx);
    if (!ret) {
        LOG(" failed to get context %d\n", nodeToExpand);
        return false;
    }

    toExpand.push_back(nodeToExpand);
    ret = viewer_set_groups_visibility(ctx.tcontrol, toExpand, true);
    if(ret) {
        DEBUGLOG("Success at expanding group\n");
    } else {
        LOG("Failed at expanding group\n");
    }
    return -1;
}


// handle a request to complement the current graph
bool complementCurrentSubgraph() {
    GraphHelpContext ctx;
    bool ret = fillContext(ctx);
    if (!ret) {
        return false;
    }
    if (ctx.currNode < 0) {
        msg("%s: Node not selected\n", PLUGINNAME);
        return false;
    }
    DEBUGLOG("Using selected node: %d\n", ctx.currNode);
    if (!ctx.mgraph->is_subgraph_node(ctx.currNode)) {
        msg("%s: Current node is not a member of a group\n", PLUGINNAME);
        return true;
    }
    int currentGroup = ctx.mgraph->get_node_group(ctx.currNode);
    DEBUGLOG("Current group: %d\n", currentGroup);

    // Note: originally i explicitly collapsed all of the currently expanded
    // groups in the fucntion (except for the currentGroup). This lead to odd
    // behavior where the view wouldn't refresh. running idc.Refresh() from
    // the interactive prompt correctly refreshed the view and shows what i
    // expected. adding in calls to refresh_idaview_anyway and auto_wait didn't 
    // resolve the issue. it turns out that things actually work fine without 
    // collapsing  existing groups.

    group_crinfo_t newGroup;
    groups_crinfos_t newGroups;
    for(int i = 0; i < ctx.mgraph->size(); ++i) {
        if (!ctx.mgraph->is_visible_node(i)) {
            continue;
        }
        if (ctx.mgraph->get_node_group(i) == currentGroup) {
            continue;
        }
        //DEBUGLOG("Adding node to new group: %d\n", i);
        newGroup.nodes.push_back(i);
    }
    newGroup.text.sprnt("Complement of group %d", currentGroup);
    newGroups.push_back(newGroup);
    intvec_t out_group_nodes;

    ret = viewer_create_groups(ctx.tcontrol, &out_group_nodes, newGroups);
    if(ret) {
        //auto_wait();
        //refresh_idaview_anyway();
        DEBUGLOG("%s: complement group %d success.\n", PLUGINNAME, currentGroup);
    } else {
        msg("%s: Unable to complement group %d.\n", PLUGINNAME, currentGroup);
    }
    return true;
}

// handle a request to complement the current group node
bool complementCurrentGroup() {
    GraphHelpContext ctx;
    bool ret = fillContext(ctx);
    if (!ret) {
        return false;
    }
    if (ctx.currNode < 0) {
        msg("%s: Node not selected\n", PLUGINNAME);
        return false;
    }
    int currGroup = ctx.currNode;
    DEBUGLOG("Using selected group node: %d\n", ctx.currNode);
    if (!ctx.mgraph->is_group_node(ctx.currNode)) {
        // this shouldn't happen
        msg("%s: Current node is a group node\n", PLUGINNAME);
        return true;
    }

    group_crinfo_t newGroup;
    groups_crinfos_t newGroups;
    for(int i = 0; i < ctx.mgraph->size(); ++i) {
        // select everything that is visible other than the current group node
        if (i == ctx.currNode) {
            continue;
        }
        if (!ctx.mgraph->is_visible_node(i)) {
            continue;
        }
        //DEBUGLOG("Adding node to new group: %d\n", i);
        newGroup.nodes.push_back(i);
    }
    newGroup.text.sprnt("Complement of group %d", ctx.currNode);
    newGroups.push_back(newGroup);
    intvec_t out_group_nodes;

    ret = viewer_create_groups(ctx.tcontrol, &out_group_nodes, newGroups);
    if(ret) {
        DEBUGLOG("%s: complement group %d success.\n", PLUGINNAME, ctx.currNode);

        // crap. ida crashes if you try to do any other graph modifications
        // immediately here. registering a hard-coded timer callback to defer
        // expanding the graph node we're interested in. No HT_GRAPH 
        // graph_notification_t below 256 are fired when I hook_to_notification()
        // of the IDA graph view (so the grcode_changed_graph event is not accessible). 
        //
        // Other hacks/work-arounds I tried were ui_request_t
        // , exec_request_t, trying to check auto analysis queue, trying on subsequent 
        // qflow_chart_t HT_IDP and HT_UI callbacks, 

        // XXX TODO orange
        // SUPER HACKY ATTEMPT TO GET AROUND UNKNOWN WAY TO DO GRAPH MODIFICATIONS
        // DURING GRAPH ANIMATION, OR QUEUE WHEN ANIMATION COMPLETES. I TRIED SO MANY
        // OTHER THINGS, AND NOTHING ELSE WORKED. DON'T BLAME ME IF THIS FAILS.
        register_timer(1000, complementCallback, (void*)(ctx.currNode));

    } else {
        msg("%s: Unable to complement group %d.\n", PLUGINNAME, ctx.currNode);
    }
    return true;
}

// handle a switch branch node with multiple successors
bool handleSwitchStatementCollapse() {
    GraphHelpContext ctx;
    bool ret = fillContext(ctx);
    if (!ret) {
        return false;
    }
    if (ctx.currNode < 0) {
        msg("%s: Node not selected\n", PLUGINNAME);
        return false;
    }
    Flare::FlareGraph fg;
    ret = loadFlareGraphFromIda(ctx, fg, true);
    if (!ret) {
        msg("%s: Unable to load FlareGraph from IDA\n", PLUGINNAME);
        return false;
    }
    if (fg.getRootNode() < 0) {
        msg("%s: No head node selected!\n", PLUGINNAME);
        return false;
    }
    groups_crinfos_t newGroups;
    for (int i = 0; i < ctx.mgraph->nsucc(ctx.currNode); ++i) {
        int succ = ctx.mgraph->succ(ctx.currNode, i);
        if (!ctx.mgraph->is_visible_node(succ)) {
            // skip non visible nodes
            continue;
        }
        DEBUGLOG("%s: Exploring node %d (%d)\n", PLUGINNAME, succ, i);
        ret = handleSwitchCaseCollapse(ctx, fg, succ, newGroups);
        if (!ret) {
            msg("%s: Switch statement bailing out due to case error\n", PLUGINNAME);
            return false;
        }
    }
    if (newGroups.size() == 0) {
        msg("%s: Skipping new group creation. Got 0 new groups for switch node\n", PLUGINNAME);
    } else {
        DEBUGLOG("Do create groups now\n");
        intvec_t out_group_nodes;
        ret = viewer_create_groups(ctx.tcontrol, &out_group_nodes, newGroups);
        if (ret) {
            msg("%s: Created subgraphs for %d switch cases\n", PLUGINNAME, newGroups.size());
        } else {
            msg("%s: New groups failed :(\n", PLUGINNAME);
        }

    }
    return true;
}


#if IDA_SDK_VERSION < 700

// Attempts to get the text associated with a given EA, trying comments and 
// visible names
bool getEaText(ea_t ea, qstring &nodetext) {
    ssize_t cmtsize = 0;
    char locbuff[BUFFSIZE] = { 0 };
    cmtsize = get_cmt(ea, true, locbuff, BUFFSIZE);
    if (cmtsize > 0) {
        nodetext = qstring(locbuff);
        DEBUGLOG("Got rpt comment: '%s'\n", nodetext.c_str());
        return true;
    }
    cmtsize = get_cmt(ea, false, locbuff, BUFFSIZE);
    if (cmtsize > 0) {
        nodetext = qstring(locbuff);
        DEBUGLOG("Got nrpt comment: '%s'\n", nodetext.c_str());
        return true;
    }
    cmtsize = get_cmt(ea, false, locbuff, BUFFSIZE);
    if (cmtsize > 0) {
        nodetext = qstring(locbuff);
        DEBUGLOG("Got visible name: '%s'\n", nodetext.c_str());
        return true;
    }
    return false;
}

#else  // #if IDA_SDK_VERSION < 700

bool getEaText(ea_t ea, qstring &nodetext) {
    ssize_t cmtsize = 0;
    cmtsize = get_cmt(&nodetext, ea, true);
    if (cmtsize > 0) {
        DEBUGLOG("Got rpt comment: '%s'\n", nodetext.c_str());
        return true;
    }
    cmtsize = get_cmt(&nodetext, ea, false);
    if (cmtsize > 0) {
        DEBUGLOG("Got nrpt comment: '%s'\n", nodetext.c_str());
        return true;
    }
    cmtsize = get_visible_name(&nodetext, ea);
    if (cmtsize > 0) {
        DEBUGLOG("Got visible name: '%s'\n", nodetext.c_str());
        return true;
    }
    return false;
}

#endif // #if IDA_SDK_VERSION < 700

// attempts to get the representative text for a given switch case
bool getSwitchCaseText(GraphHelpContext &ctx, Flare::FlareGraph &fg, int node, qstring &nodetext) {
    ssize_t cmtsize = 0;
    char locbuff[BUFFSIZE] = { 0 };
    if (node < g_lastFlowGraph.getNodeCount()) {
        ea_t blockea =  g_lastFlowGraph.getNodeAreaStart(node);
        DEBUGLOG("Getting comment from block %d: 0x%08"EAFMTSTR"\n", node, blockea);
        bool ret = getEaText(blockea, nodetext);
        if (ret) {
            return true;
        }
    }

    DEBUGLOG("Falling back to crappy switch label\n");
    qsnprintf(locbuff, BUFFSIZE, "node %d", node); 
    nodetext = qstring(locbuff);
    return true;
}

// create a new group for an individual switch case
bool handleSwitchCaseCollapse(GraphHelpContext &ctx, Flare::FlareGraph &fg, int node, groups_crinfos_t &newGroups) {
    DEBUGLOG("%s: changing root node from %d to %d\n", PLUGINNAME, fg.getRootNode(), node);
    fg.clearNodeProp(fg.getRootNode(), Flare::NodeProperty::Root);
    fg.addNodeProp(node, Flare::NodeProperty::Root);
    Flare::intset_t ureach;
    //return true;
    bool ret = fg.uniqueReachable(ureach);
    if (!ret) {
        msg("%s: Failed to get unique reachable for switch case \n", PLUGINNAME);
        return false;
    }
    if (ureach.size() == 0) {
        // TODO: right now i'm making a group even if there's only 1 node in the UR 
        // switch case. should i check for this?
        msg("%s: Skipping case for node %d. Received 0 nodes for new group\n", PLUGINNAME, node);
    } else {
        // ok, create a group
        group_crinfo_t newGroup;
        for (auto it = ureach.begin(); it != ureach.end(); ++it) {
            newGroup.nodes.push_back(*it);
        }
        DEBUGLOG("%s: Creating new switch group %d: %d of %d total nodes\n", PLUGINNAME, node, newGroup.nodes.size(), fg.getNodeCount()); 
        char nodetext[BUFFSIZE] = { 0 } ;
        getSwitchCaseText(ctx, fg, node, newGroup.text);
        newGroups.push_back(newGroup);
    }
    return true;
}

// print information about nodes. used to infer what the graph data actually means
void printNodeInfo(const GraphHelpContext &ctx, int node, bool doSubgraphs) {
    msg("printNodeInfo: %d\n", node);
    if (node < 0) {
        msg("Negative node number\n");
        return;
    }
    if (node >= ctx.mgraph->size()) {
        msg("Node too big\n");
        return;
    }
#if 0  // leaving this dead code block in as a reference for the future of what i tried, but ida didn't like
    node_info_t ninfo1;
    msg("ctx.mgraph->gid: %08"EAFMTSTR"\n", ctx.mgraph->gid);
    // apprently all of the node_info funcs are broken, or they don't work on the built-in ida graph
    bool ret = get_node_info2(&ninfo1, ctx.mgraph->gid, node);
    if (ret) {
        msg("get_node_info2 worked\n");
    } else {
        msg("Failed at get_node_info2\n");
    }
    msg("NodeInfo %8d: (valid_flags: %04x)(bg %08x)(frame %08x)(ea %08"EAFMTSTR")'%s'\n", 
            node, ninfo1.get_flags_for_valid(), ninfo1.bg_color, 
            ninfo1.frame_color, ninfo1.ea, ninfo1.text.c_str()
       );

    node_info_t ninfo2;
    ret = viewer_get_node_info(ctx.tcontrol, &ninfo2, node);
    if (ret) {
        msg("viewer_get_node_info worked\n");
    } else {
        msg("Failed at viewer_get_node_info\n");
    }
    msg("NodeInfo %8d: (bg %08x)(frame %08x)(ea %08"EAFMTSTR")'%s'\n", node, 
            ninfo2.bg_color, ninfo2.frame_color, ninfo2.ea, ninfo2.text.c_str()
       );

    bgcolor_t nodecolor = 0;
    ea_t nodeea = 0;
    char* oldninfo = get_node_info(ctx.mgraph->gid, node, &nodecolor, &nodeea);
    if (oldninfo == NULL) {
        msg("Failed at deprecated get_node_info\n");
    } else {
        // orange TODO: comment says that i have to free returned data
        msg("NodeInfo %8d: (bg %08x)(ea %08"EAFMTSTR")'%s'\n", node, nodecolor, nodeea, oldninfo);
    }
#endif
    // trying to interpret graph information available from ida...
    msg("  is_subgraph_node:    %d\n", ctx.mgraph->is_subgraph_node(node));  // returns true if this node is a a member of a group
    msg("  is_dot_node:         %d\n", ctx.mgraph->is_dot_node(node));
    msg("  is_group_node:       %d\n", ctx.mgraph->is_group_node(node));    // returns true if this node represents a collapsed group
    msg("  is_displayable_node: %d\n", ctx.mgraph->is_displayable_node(node));
    msg("  is_simple_node:      %d\n", ctx.mgraph->is_simple_node(node));   // not a group node (?)
    msg("  is_collapsed_node:   %d\n", ctx.mgraph->is_collapsed_node(node)); // true if this is a group  node, and currently collapsed
    msg("  is_uncollapsed_node: %d\n", ctx.mgraph->is_uncollapsed_node(node));
    msg("  is_visible_node:     %d\n", ctx.mgraph->is_visible_node(node));
    msg("  node_repr:           %d\n", ctx.mgraph->get_node_representative(node));  // the node number, or if it's part of collapsed group, gives the group node
    msg("  node_group:          %d\n", ctx.mgraph->get_node_group(node));   // the group this node belongs to. if not part of a group, returns its own node number
    msg("  is_deleted_node:     %d\n", ctx.mgraph->is_deleted_node(node));
    // when a subgraph node is not visible, it has no successors or predecessors
    msg("Node %d has %d successors:\n ", node, ctx.mgraph->nsucc(node));
    for (int i = 0; i < ctx.mgraph->nsucc(node); i++) {
        msg("  %3d", ctx.mgraph->succ(node, i));
    }
    msg("\n");
    msg("Node %d has %d predecessors:\n ", node, ctx.mgraph->npred(node));
    for (int i = 0; i < ctx.mgraph->npred(node); i++) {
        msg("  %3d", ctx.mgraph->pred(node, i));
    }
    msg("\n");
    if (doSubgraphs) {
        if (ctx.mgraph->is_group_node(node)) {
            msg("Node has subgraphs:\n");
            int subnode = ctx.mgraph->get_first_subgraph_node(node);
            while(subnode != -1) {
                msg("   %3d: recursing\n", subnode);
                printNodeInfo(ctx, subnode);
                subnode = ctx.mgraph->get_next_subgraph_node(node, subnode);
            }
        }
        msg("\n");
    }
}

// debug routine
void printGraphSelection(GraphHelpContext &ctx) {
    screen_graph_selection_t graphSel;
    bool ret = viewer_get_selection(ctx.tcontrol, &graphSel);
    if (!ret) {
        msg("Failed to get graph selection\n");
    } else {
        msg("Printing graph selection now\n");
        for (screen_graph_selection_t::const_iterator p=graphSel.begin(); p != graphSel.end(); ++p ) {
            if (p->is_node) {
                msg("   node: %d\n", p->node);
            } else {
                msg("   not a node\n");
            }
        }
        msg("Done pringint selection\n");
    }
}

// Note: this isn't being actively used at the moment. Only used during initial PoC and research
bool collapsePostDomNodes(GraphHelpContext &ctx) {
    screen_graph_selection_t graphSel;
    intvec_t out_group_nodes;
    Flare::FlareGraph fg;
    bool ret = viewer_get_selection(ctx.tcontrol, &graphSel);
    if (!ret) {
        msg("%s: Failed to get graph selection\n", PLUGINNAME);
        return false;
    }
    if (graphSel.size() != 1) {
        msg("%s: One item must be selected: the first node to start searching for a post-dominator\n", PLUGINNAME);
        return false;
    }
    ret = loadFlareGraphFromIda(ctx, fg, false);
    if (!ret) {
        msg("%s: Unable to load graph from IDA\n", PLUGINNAME);
        return false;
    }
    if (fg.getRootNode() < 0) {
        msg("%s: No head node selected!\n", PLUGINNAME);
        return false;
    }
    DEBUGLOG("Using %d as the root node with the following sentry:\n    ", fg.getRootNode());

    Flare::intset_t pdomgroup;
    ret = fg.getpdomGroup(pdomgroup, fg.getRootNode());
    if (!ret) {
        msg("%s: Failed to find post dominator grouping \n", PLUGINNAME);
        return false;
    }
    if (pdomgroup.size() == 0) {
        msg("%s: Bailing out. Received 0 nodes for new group\n", PLUGINNAME);
        return false;
    }
    return createGroup(ctx, fg, pdomgroup);
}

// i can't find a way to hook for notifications of when the user navigates to a 
// new function, so abusing the HT_UI and HT_IDP  callbacks that provide qflow_chart_t's
bool handleChangedView(qflow_chart_t *newfc) {
    bool doReload = false;

    if (g_lastFlowGraph.getNodeCount() != newfc->size()) {
        DEBUGLOG("closing chooser (size): g_lastFlowGraph-: %d vs newfc: %d\n", g_lastFlowGraph.getNodeCount(), newfc->size());
        doReload = true;
    } else if ((g_lastFlowGraph.getNodeCount() == 0) || (newfc->size() == 0)) {
        DEBUGLOG("closing chooser (size): g_lastFlowGraph-: %d vs newfc: %d\n", g_lastFlowGraph.getNodeCount() , newfc->size());
        doReload = true;
    } else if (g_lastFlowGraph.getFunctionEa() != newfc->blocks[0].STARTEA) {
        DEBUGLOG("closing chooser (ea): g_lastFlowGraph-: %08"EAFMTSTR" newfc: %08"EAFMTSTR"\n", g_lastFlowGraph.getFunctionEa(), newfc->blocks[0].STARTEA);
        doReload = true;
    }
    if (doReload) {
        loadCurrentFlowChart(newfc);
        SubgraphChooser::staticClearRefreshChooser(true);
    }
    return true;
}

bool collapseReachableUniqueNodes(GraphHelpContext &ctx) {
    screen_graph_selection_t graphSel;
    Flare::FlareGraph fg;
    bool ret = viewer_get_selection(ctx.tcontrol, &graphSel);
    if (!ret) {
        msg("%s: Failed to get graph selection\n", PLUGINNAME);
        return false;
    }
    if (graphSel.size() == 0) {
        msg("%s: At least one items must be selected: the first node to start, and zero or more nodes act as sentry nodes\n", PLUGINNAME);
        return false;
    }
    ret = loadFlareGraphFromIda(ctx, fg, false);
    if (!ret) {
        msg("%s: Unable to load FlareGraph from IDA\n", PLUGINNAME);
        return false;
    }
    if (fg.getRootNode() < 0) {
        msg("%s: No head node selected!\n", PLUGINNAME);
        return false;
    }
    Flare::intset_t ureach;
    ret = fg.uniqueReachable(ureach);
    if (!ret) {
        msg("%s: Failed to get unique reachable\n", PLUGINNAME);
        return false;
    }
    if (ureach.size() == 0) {
        msg("%s: Bailing out. Received 0 nodes for new group\n", PLUGINNAME);
        return false;
    }
    return createGroup(ctx, fg, ureach);
}

void idaapi runUniqueReachable(void) {
    GraphHelpContext ctx;
    bool ret = fillContext(ctx);
    if (!ret) {
        return;
    }
    if (ctx.currNode < 0) {
        msg("%s: Node not selected\n", PLUGINNAME);
        return;
    }

    // verify the flowchart here
    if (g_lastFlowGraph.getNodeCount() > ctx.mgraph->size()) {
        msg("%s: Saved FlowChart larger than current graph. Nope\n", PLUGINNAME);
        return;
    }

    DEBUGLOG("%s: Got mutable_graph %08"EAFMTSTR": %d nodes\n", PLUGINNAME, ctx.mgraph->gid, ctx.mgraph->size());
    DEBUGEXEC(printGraphSelection(ctx); );
    DEBUGEXEC(printNodeInfo(ctx, ctx.currNode); );
    collapseReachableUniqueNodes(ctx);
    g_lastFlowGraph.clear();
    return;
}

//-------------------------------------------------------------------------
// plugin book-keeping

void readConfig() {
    DEBUGLOG("%s: Reading %s config now\n", PLUGINNAME, MY_CONFIG_FILE);
    bool ret = read_config_file(MY_CONFIG_FILE, g_opts, qnumber(g_opts));
    if (ret) {
        DEBUGLOG("%s: Read config success. Values now: %d %d 0x%08x\n", PLUGINNAME, 
            g_minimumSubgraphNodeCount, g_maximumSubgraphNodePercentage, g_highLightNodeColor
        );
    } else {
        DEBUGLOG("%s: Read config failed. Values now: %d %d 0x%08x\n", PLUGINNAME,
            g_minimumSubgraphNodeCount, g_maximumSubgraphNodePercentage, g_highLightNodeColor
        );
    }
}

void registerAddon() {
    addon_info_t addon_info;
    addon_info.cb = sizeof(addon_info_t);
    addon_info.id = "com.fireeye.flare.simplifygraph";
    addon_info.name = "SimplifyGraph";
    addon_info.producer = "FireEye FLARE";
    addon_info.version = "0.1";
    addon_info.url = "https://github.com/fireeye/SimplifyGraph";
    addon_info.freeform = "Copyright (c) 2017 FireEye";
    register_addon(&addon_info);
}


//-------------------------------------------------------------------------
// callbacks/hooks

static ssize_t idaapi view_callback(void * ud, int notification_code, va_list va) {
    switch ( notification_code ) {
        case view_switched: {
#if IDA_SDK_VERSION < 700
            TForm* tform = get_current_tform();
            if (get_tform_type(tform) != BWN_DISASM) {
                DEBUGLOG("%s: Not a disssembly tform\n", PLUGINNAME);
                return 0;
            }
            TCustomControl *view = va_arg(va, TCustomControl *);

#else

            TWidget *view = va_arg(va, TWidget *);
            if (get_widget_type(view) != BWN_DISASM) {
                DEBUGLOG("%s: Not a disssembly tform\n", PLUGINNAME);
                return 0;
            }

#endif
            tcc_renderer_type_t rt = va_arg(va, tcc_renderer_type_t);
            SubgraphChooser::staticClearRefreshChooser();
            //if (rt != TCCRT_GRAPH) {
            //    DEBUGLOG("No longer graph\n");
            //    SubgraphChooser::staticClearRefreshChooser();
            //    //SubgraphChooser::staticCloseChooser();
            //}
        }
        break;
//        case view_created: {
//                LOG("view_created\n");
//#if IDA_SDK_VERSION < 700
//
//                TCustomControl *view = va_arg(va, TCustomControl *);
//
//#else  // #if IDA_SDK_VERSION < 700
//
//                TWidget *view = va_arg(va, TWidget *);
//
//#endif // #if IDA_SDK_VERSION < 700
//                if (is_idaview(view) && ((get_view_renderer_type(view) == TCCRT_GRAPH)) || (get_view_renderer_type(view) == TCCRT_FLAT)) {
//                    LOG("   ida disass\n");
//                }
//            }
//            break;
//
//        case view_close: {
//                LOG("view_close\n");
//#if IDA_SDK_VERSION < 700
//
//                TCustomControl *view = va_arg(va, TCustomControl *);
//
//#else  // #if IDA_SDK_VERSION < 700
//
//                TWidget *view = va_arg(va, TWidget *);
//
//#endif // #if IDA_SDK_VERSION < 700
//                if (is_idaview(view) && ((get_view_renderer_type(view) == TCCRT_GRAPH)) || (get_view_renderer_type(view) == TCCRT_FLAT)) {
//                    LOG("   ida disass\n");
//                }
//            }
//            break;
//
    }
    return 0;
}
//#endif // #if IDA_SDK_VERSION < 700

static ssize_t idaapi ui_callback(void * ud, int notification_code, va_list va) {
    switch ( notification_code ) {

#if IDA_SDK_VERSION < 700
        case ui_tform_invisible: {
                TForm *tform = va_arg(va, TForm *);
                if (get_tform_type(tform) == BWN_DISASM) {
                    DEBUGLOG("ui_tform_invisible BWN_DISASM\n");
                    SubgraphChooser::staticClearRefreshChooser();
                    //SubgraphChooser::staticHandleTFormChanged(tform);
                }
            }
            break;
#else
        case ui_widget_invisible: {
                TWidget *twidget = va_arg(va, TWidget *);
                if (get_widget_type(twidget) == BWN_DISASM) {
                    DEBUGLOG("ui_tform_invisible BWN_DISASM\n");
                    SubgraphChooser::staticClearRefreshChooser();
                    //SubgraphChooser::staticHandleTFormChanged(tform);
                }
            }
            break;

#endif // #if IDA_SDK_VERSION < 700

        case UI_POPULATING_POPUP: {
#if IDA_SDK_VERSION < 700

            TForm *f = va_arg(va, TForm *);

#else  // #if IDA_SDK_VERSION < 700

            TWidget *f = va_arg(va, TWidget *);

#endif // #if IDA_SDK_VERSION < 700

            if (GET_TFORM_TYPE(f) == BWN_DISASM) {
                TPopupMenu *p = va_arg(va, TPopupMenu *);

#if IDA_SDK_VERSION < 700

                TCustomControl *view = get_tform_idaview(f);

#else  // #if IDA_SDK_VERSION < 700

                TWidget *view = f;

#endif // #if IDA_SDK_VERSION < 700

                if ( ( view != NULL ) &&  ( get_view_renderer_type(view) == TCCRT_GRAPH ) ) {
                    attach_action_to_popup(f, p, ACTION_NAME_COLLAPSE_UNIQUE_REACH, POPUPPATH);
                    // no longer allowing straight collapse to post dominator actions. 
                    // now wrapped in the isolated subgraph chooser
                    //attach_action_to_popup(f, p, ACTION_NAME_COLLAPSE_POST_DOM, POPUPPATH);
                    DEBUGEXEC({
                        // just dump graphs to dot format during debug mode    
                        attach_action_to_popup(f, p, ACTION_NAME_CREATE_DOTFILE, POPUPPATH);
                    });
                    if (isSubgroupNode()) {
                        attach_action_to_popup(f, p, ACTION_NAME_COMPLEMENT_CURRENT_GRAPH, POPUPPATH);
                    }
                    if (isGroupNode()) {
                        attach_action_to_popup(f, p, ACTION_NAME_COMPLEMENT_EXPAND_CURRENT_GRAPH, POPUPPATH);
                    }

                    // moved this to be invoked when plugin run() is called
                    //if (!SubgraphChooser::staticIsActive()) {
                    //    attach_action_to_popup(f, p, ACTION_NAME_DISCOVER_ISOLATED_SUBGRAPHS, POPUPPATH);
                    //}
                    if (isSwitchStatementNode()) {
                        attach_action_to_popup(f, p, ACTION_NAME_COLLAPSE_SWITCH_CASES, POPUPPATH);
                    }
                    int currNode = viewer_get_curnode(view);
                    if ((currNode >= 0) && (SubgraphChooser::isNodeActivelyHighlighted(currNode))) {
                        attach_action_to_popup(f, p, ACTION_NAME_COLLAPSE_CURRENT_SUBGRAPH, POPUPPATH);
                    }
                }
            }
        }
        break;
        case ui_gen_idanode_text: { 
            // cb: generate disassembly text for a node
            // just another place to grab a fresh flow chart if needed later
            // feels really dirty, but only way i've been able to find to access it :(
            qflow_chart_t *fc = va_arg(va, qflow_chart_t *);
            handleChangedView(fc);
            return 0;
        }
        break;
    }
    return 0;
}

#if 0
// TO REMOVE: WHEN UPGRADING IDB FORMATS, BAD FLOWCHARTS ARE SENT THIS WAY
// calling any member function on them would just crash ida.
// MAYBE WE CAN JUST RELY ON HT_UI HOOKS
static ssize_t idaapi idp_callback(void *, int code, va_list va) {
    switch ( code ) {
        case PROCESS_FLOWCHART: {
            // gui has retrieved a function flow chart
            // just another place to grab a fresh flow chart if needed later
            // feels really dirty, but only way i've been able to find to access it :(
            qflow_chart_t *fc = va_arg(va, qflow_chart_t *);
            handleChangedView(fc);
        }
        break;
    }
    return 0;
}
#endif

bool hooked = false;

void setupHooksAndActions(void) {
    DEBUGLOG("%s: setupHooksAndActions\n", PLUGINNAME);
    // set callback for view notifications 
    if ( !hooked ) {
        hook_to_notification_point(HT_UI, ui_callback, NULL);

//#if IDA_SDK_VERSION < 700

        hook_to_notification_point(HT_VIEW, view_callback, NULL);

//#endif // #if IDA_SDK_VERSION < 700

        //hook_to_notification_point(HT_IDP, idp_callback, NULL);

        hooked = true;
        DEBUGLOG("%s: installed view notification hook.\n", PLUGINNAME);
        // Register actions
        for ( size_t i = 0, n = qnumber(actions); i < n; ++i ) {
            register_action(actions[i]);
        }
    }
}

//--------------------------------------------------------------------------
// plugin_t struct callbacks

int idaapi init(void) {
    if (is_idaq()) {
        DEBUGLOG("%s: init\n", PLUGINNAME);
        registerAddon();
        readConfig();
        setupHooksAndActions();
        return PLUGIN_KEEP;
    }

    return PLUGIN_SKIP;
}

void idaapi term(void) {
    DEBUGLOG("%s: term\n", PLUGINNAME);
    SubgraphChooser::staticDeleteChooser();
    unhook_from_notification_point(HT_UI, ui_callback, NULL);

//#if IDA_SDK_VERSION < 700

    unhook_from_notification_point(HT_VIEW, view_callback, NULL);

//#endif // #if IDA_SDK_VERSION < 700

    //unhook_from_notification_point(HT_IDP, idp_callback, NULL);
}

RUN_RET_TYPE idaapi run(RUN_ARG_TYPE) {
    DEBUGLOG("%s: run\n", PLUGINNAME);
    // make sure we can load a context, indicating that there's an active graph disassemly view
    GraphHelpContext ctx;
    bool ret = fillContext(ctx);
    if(!ret) {
        msg("%s: Cannot fill graph context. Likely not an active ida graph disassembly\n", PLUGINNAME);
        return RUN_RET;
    }
    if (!SubgraphChooser::staticIsActive()) {
        SubgraphChooser::show(); 
    }
    return RUN_RET;
}

//-------------------------------------------------------------------------

extern "C" {

plugin_t PLUGIN = {
    IDP_INTERFACE_VERSION,
    0,                    // plugin flags
    init,                 // initialize
    term,                 // terminate. this pointer may be NULL.
    run,                  // invoke plugin
    PLUGIN_COMMENT,       // long comment about the plugin
    PLUGIN_HELP,          // multiline help about the plugin
    WANTED_NAME,          // the preferred short name of the plugin
    NULL                  // the preferred hotkey to run the plugin
};

}
