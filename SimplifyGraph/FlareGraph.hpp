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

#ifndef FLARE_GRAPH
#define FLARE_GRAPH

namespace Flare {

// i'm trying to keep IDA types out of this file, so using fea_t as a stand-in for ea_t
// see also the 
#ifdef __EA64__
    typedef uint64_t fea_t;
#else
    typedef uint32_t fea_t;
#endif


    const int IPDOM_UNKNOWN = -1;
    const int IPDOM_CONFLICT = -2;

    enum class NodeProperty {
        Selected = 0,
        Pruned,
        Root,
        Sentry,
    };

    typedef std::vector<int> intvector_t;
    typedef std::set<NodeProperty> nodepropset_t;
    typedef std::set<int> intset_t;
    typedef std::deque<int> intdeque_t;
    // TODO
    //typedef std::vector<int>::const_iterator intvector_iter_t;
    typedef std::vector<intvector_t> array_of_intvector_t;
    typedef std::map<int, int> intintMap_t;
    typedef std::map<int, intset_t> intsetMap_t;
    // TODO
    //typedef std::vector<intset_t> intsetVec_t;
    typedef std::map<int, std::string> intstringMap_t;
    typedef std::pair<int, int> intintPair_t;
    typedef std::set<intintPair_t> intintPairSet_t;

    struct subgraph_t {
        int n1;
        int n2;
        intset_t nodes;

        subgraph_t(int n1, int n2) : n1(n1), n2(n2) { };
    };
    typedef std::vector<subgraph_t> subgraphVec_t;

    struct area_t {
        fea_t start;
        fea_t end;
    };
    typedef std::vector<area_t> areaVec_t;

    typedef boost::adjacency_list<
        boost::vecS,
        boost::vecS,
        boost::bidirectionalS,
        boost::property<boost::vertex_index_t, std::size_t>,
        boost::no_property> DomGraph;
    typedef std::map<int, nodepropset_t>::const_iterator nodepropiter_t;
    typedef nodepropset_t::const_iterator propiter_t;
    typedef boost::adjacency_list<
        boost::vecS,
        boost::vecS,
        boost::bidirectionalS,
        boost::property<boost::vertex_index_t, std::size_t>,
        boost::no_property> DomGraph;

    typedef boost::graph_traits<DomGraph>::vertex_descriptor Vertex;
    typedef std::map<Vertex, Vertex> VertexVertexMap;


    // some convenience functions to check existence of nodes in data structures
    bool intsetHas(const intset_t &s1, int n1);
    bool intdequeHas(const intdeque_t &s1, int n1);
    bool intvectorHas(const intvector_t &s1, int n1);

    // colors are abused in the .dot files to store properties. only used for debugging really
#define NODE_COLOR_SELECTED "lightskyblue"
#define NODE_COLOR_PRUNED "gray"
#define NODE_COLOR_ROOT "green"
#define NODE_COLOR_SENTRY "firebrick1"

    // Our own graph implementation, largely mimicking the interface to IDA's qflow_chart_t.
    // Used to store state across IDA callbacks because we can't guarantee we're not using
    // IDA's objects past their lifetime. Also
    // The Boost graph library was overly complex for my purposes. In this case, each node
    // can have an integer "name", and a few optional properties that may be used in
    // our graph calculations. The class uses an internal "name", and maps it back to the 
    // public name for public functions. Internal functions ending in 'i' like predi() are
    // using the internal name.
    //
    // This class also includes some graph algorithms used by the plugin
    class FlareGraph {

    private:
        //prevent creation of copy constructor & move
        FlareGraph(const FlareGraph& other);
        FlareGraph& operator=(const FlareGraph& other);

        // vectors of vectors of ints, tracks the predecessors and successors of each node
        array_of_intvector_t preds;
        array_of_intvector_t succs;

        // keep track of the the area_t for each node
        areaVec_t blocks;

        // map the external node name to the one we use internally
        intintMap_t nameMap;

        // map the internal node name to the external one
        intintMap_t reverseNameMap;

        // map the internal node name to a vector of properties
        std::map<int, nodepropset_t> nodeProps;
        //
        // map the internal node name to a raw color. not to be used at the same time as nodeproperties!
        intstringMap_t nodeColors;

        int rootNode;
        fea_t functionEa;
        int edgeCount;
        //int nodeCount;
        intvector_t sentryNodes;
        std::string inputPath;

        // return the internal node ID given the external name
        // returns -1 on error
        int mapNode(int name) const;

        // map the internal node name to the external name
        int reverseMapNode(int name) const;

        //internal version of succ() and pred(), name is already internal
        int succi(int name, int idx) const;
        int predi(int name, int idx) const;
        int nsucci(int name) const;
        int npredi(int name) const;

        //internal versions of reachable and uniqueReachable, uses internal names
        bool reachablei(intset_t &out);
        bool uniqueReachablei(intset_t &out);

        // TODO
        //DomGraph initDomGraph();
        bool idomHelper(DomGraph &dgr, intintMap_t &outdom, int entry);
        bool verifySimpleSubGraphs(subgraphVec_t &out, intintPairSet_t &candidates, unsigned int minNodeCount, unsigned int maxNodePercentage);
        bool isSimpleSubGraph(int n1, int n2, subgraph_t &outset, unsigned int minNodeCount, unsigned int maxNodePercentage);


    public:
        FlareGraph();
        virtual ~FlareGraph();

        // write out the current graph edges
        bool log() const;

        // explicit copy, basically
        bool load(const FlareGraph& other);

        // reset the graph to an empty state
        bool clear();

        // keep track of the associated filepath (like the idb path) for debugging/.dot file inclusion
        bool setFilePath(const char*);

        // returns number of successors for a given node
        int nsucc(int name) const;

        // returns number of predecessors for a given node
        int npred(int name) const;

        // returns the idx'th entry for name's successor
        int succ(int name, int idx) const;

        // returns the idx'th entry for name's predecessor
        int pred(int name, int idx) const;

        // set/get the EA for the funstion start
        void setFunctionEa(fea_t ea) { functionEa = ea; }
        fea_t getFunctionEa() const { return functionEa; }

        // Get the number of nodes in this graph
        int getNodeCount() const;

        // Get the number of edges in this graph
        int getEdgeCount() const { return edgeCount; }

        // Get the root node for this graph (if any).
        int getRootNode() const { return reverseMapNode(rootNode); }

        // Get the number of sentry nodes in this graph
        int getSentryCount() const { return (int)sentryNodes.size(); }

        // Get the idx'th sentry node
        int getSentry(int idx) const { return mapNode(sentryNodes[idx]); }

        // Get the area associated with a node (start/stop range)
        bool getNodeArea(int n1, area_t &area) const;

        // Helpers for just the start/end of an area for a given node
        fea_t getNodeAreaStart(int n1) const;
        fea_t getNodeAreaEnd(int n1) const;

        // apply a node color
        bool colorNode(int n1, const char* color);

        // Add and clear properties for a given node
        bool clearNodeProp(int n1, NodeProperty prop);
        bool addNodeProp(int n1, NodeProperty prop);

        // returns True if an edge n1->n2 exists (n2 is in n1's successor set)
        bool hasSucc(int n1, int n2) const;

        // returns True if an edge n1->2 exists (n1 in n2's predecessor set)
        bool hasPred(int n1, int n2) const;

        // orange TODO: add DFS visitor helper
        // orange TODO: add graphviz load

        // load & store .dot file (with our own format extensions abusing colors for properties
        bool loadGraphViz(std::istream &in);
        bool storeGraphViz(std::ostream &os);

        // Adds a new node with the given external name. External users
        // can continue to use this name to refer to the node, but the actual
        // node value may be different in the internal access
        // If name is not specified, the internal name is returned
        int addNode(int name = -1);

        // Sets the area bounds for a given node
        bool setNodeArea(int name, fea_t start, fea_t end);

        // Add a directed edge from n1 to n2, where n1 & n2 are the external
        // names previously add
        bool addEdge(int n1, int n2);

        // TODO: allow string names as well
        //bool addNode(const std::string &name);
        //bool addEdge(const std::string &n1, const std::string &n2);


        // returns the set of nodes reachable from the currently set root node
        bool reachable(intset_t &out);

        // returns the set of nodes uniquely-reachable from the currently set root node
        bool uniqueReachable(intset_t &out);

        // returns the immediate dominator tree as an int-int map
        // output nodes are the public names
        bool idom(intintMap_t &out, int entry);

        // returns the immediate post-dominator tree as an int-int map
        // entry is the node to start examining. this should be a termal
        // node in the normal group.
        bool ipdom(intintMap_t &outdom, int entry);

        // analyze the graph starting at entry, looking for an immediate
        // postdominator for node entry. returns the set of nodes also postdominated
        // by entry's immediate postdominator
        bool getpdomGroup(intset_t &out, int entry);

        // add the 'selected' node property to the given nodes
        bool selectNodes(intset_t &nodes);

        // analyze the graph for groupings based on identical ipdoms
        // performs analysis from every terminal and removes incosistent
        // nodes.
        // key of out is the ipdom, and the set of nodes are the group
        bool getipdomMap(intsetMap_t &out, bool doipdom);

        // gets the immediate dominator (or postdominator) tree by iterating over
        // all entry points (or terminals) and throwing out inconsistencies
        // The intintMap_t out contains public names of nodes (not internal)
        bool getDomIntMap(intintMap_t &out, bool doipdom);


        // top-level function to find isolated subgraphs of the current graph with
        // the given min/max parameters.
        bool findSimpleSubGraphs(subgraphVec_t &out, unsigned int minNodeCount, unsigned int maxNodePercentage);
    };
};

#endif //#ifndef FLARE_GRAPH
