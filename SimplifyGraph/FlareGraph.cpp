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

#include "FlareGraph.hpp"
#include "idalogging.h"


namespace Flare {
    class InvalidIsolatedException: public std::exception {
        virtual const char* what() const throw() {
            return "Not a valid isolated subgraph";
        }
    } myex;


    bool intsetHas(const intset_t &s1, int n1) { return s1.find(n1) != s1.end(); }
    bool intdequeHas(const intdeque_t &s1, int n1) { return std::find(s1.begin(), s1.end(), n1) != s1.end(); }
    bool intvectorHas(const intvector_t &s1, int n1) { return std::find(s1.begin(), s1.end(), n1) != s1.end(); }


    FlareGraph::FlareGraph() : rootNode(-1), functionEa(-1), edgeCount(0) {
    }

    FlareGraph::~FlareGraph() {

    }

    bool FlareGraph::clear() {
        preds.clear();
        succs.clear();
        nameMap.clear();
        reverseNameMap.clear();
        nodeProps.clear();
        nodeColors.clear();
        rootNode = -1;
        functionEa = -1;
        edgeCount = 0;
        sentryNodes.clear();
        blocks.clear();
        return true;
    }

    bool FlareGraph::log() const {
        for (int i = 0; i < getNodeCount(); ++i) {
            for (int j = 0; j < nsucc(i); j++) {
                int sn = succ(i, j);
                DEBUGLOG("  %d -> %d\n", i, sn);
            }
        }
        return true;
    }

    bool FlareGraph::load(const FlareGraph& other) {
        clear();
        area_t area;
        for (int i = 0; i < other.getNodeCount(); ++i) {
            addNode(i);
            for (int j = 0; j < other.nsucc(i); j++) {
                int succ = other.succ(i, j);
                addNode(succ);
                addEdge(i, succ);
            }
            other.getNodeArea(i, area);
            setNodeArea(i, area.start, area.end);
        }
        setFunctionEa(other.getFunctionEa());
        return true;
    }

    bool FlareGraph::setFilePath(const char* path) {
        inputPath = path;
        return true;
    }

    int FlareGraph::mapNode(int name) const {
        auto it = nameMap.find(name);
        if (it == nameMap.end()) {
            return -1;
        }
        return it->second;
    }

    int FlareGraph::reverseMapNode(int name) const {
        auto it = reverseNameMap.find(name);
        if (it == reverseNameMap.end()) {
            return -1;
        }
        return it->second;
    }

    int FlareGraph::nsucci(int in1) const {
        return (int)(succs[in1].size());
    }

    int FlareGraph::npredi(int in1) const {
        return (int)(preds[in1].size());
    }

    int FlareGraph::nsucc(int name) const {
        int in1 = mapNode(name);
        if (in1 < 0) {
            return -1;
        }
        return nsucci(in1);
    }

    int FlareGraph::npred(int name) const {
        int in1 = mapNode(name);
        if (in1 < 0) {
            return -1;
        }
        return npredi(in1);
    }

    int FlareGraph::succi(int in1, int idx) const {
        if ((unsigned int)idx >= succs[in1].size()) {
            return -1;
        }
        return succs[in1][idx];
    }

    int FlareGraph::predi(int in1, int idx) const {
        if ((unsigned int)idx >= preds[in1].size()) {
            return -1;
        }
        return preds[in1][idx];
    }

    int FlareGraph::succ(int name, int idx) const {
        int in1 = mapNode(name);
        if (in1 < 0) {
            return -1;
        }
        return reverseMapNode(succi(in1, idx));
    }

    int FlareGraph::pred(int name, int idx) const {
        int in1 = mapNode(name);
        if (in1 < 0) {
            return -1;
        }
        return reverseMapNode(predi(in1, idx));
    }

    int FlareGraph::getNodeCount() const {
        return (int)(succs.size());
    }

    bool FlareGraph::getNodeArea(int n1, area_t &area) const {
        int in1 = mapNode(n1);
        if (in1 < 0) {
            // bad names
            return false;
        }
        area.start = blocks[in1].start;
        area.end = blocks[in1].end;
        return true;
    }

    fea_t FlareGraph::getNodeAreaStart(int n1) const {
        int in1 = mapNode(n1);
        if (in1 < 0) {
            // bad names
            return false;
        }
        return blocks[in1].start;
    }

    fea_t FlareGraph::getNodeAreaEnd(int n1) const {
        int in1 = mapNode(n1);
        if (in1 < 0) {
            // bad names
            return false;
        }
        return blocks[in1].end;
    }

    // returns True if an edge n1->n2 exists (in n1's successor set)
    bool FlareGraph::hasSucc(int n1, int n2) const {
        int in1 = mapNode(n1);
        int in2 = mapNode(n2);

        if ((in1 < 0) || (in2 < 0)) {
            // bad names
            return false;
        }
        if (std::find(succs[in1].begin(), succs[in1].end(), in2) == succs[in1].end()) {
            // edge n1->n2 is NOT in in1's successor set
            return false;
        }
        return true;
    }

    // returns True if an edge n1->2 exists (in n2's predecessor set)
    bool FlareGraph::hasPred(int n1, int n2) const {
        int in1 = mapNode(n1);
        int in2 = mapNode(n2);

        if ((in1 < 0) || (in2 < 0)) {
            // bad names
            return false;
        }
        if (std::find(preds[in2].begin(), preds[in2].end(), in1) == preds[in2].end()) {
            // edge n1->n2 NOT in in2's predecessor set1
            return false;
        }
        return true;
    }

    // add a new node with the given public name. an internal name is used (mostly)
    // throughout all of the functions, mapping back to the public name when appropriate.
    int FlareGraph::addNode(int name) {
        if (name < 0) {
            LOG("Bad name: %d", name);
            return -1;
        }
        if (mapNode(name) != -1) {
            // name already exists
            DEBUGLOG("node %d already exists: %d\n", name, mapNode(name));
            return nameMap[name];
        }
        intvector_t newveca;
        intvector_t newvecb;
        succs.push_back(newveca);
        preds.push_back(newvecb);
        area_t a1 = { 0 };
        blocks.push_back(a1);
        int newNode = (int)(succs.size()) - 1;
        nameMap[name] = newNode;
        reverseNameMap[newNode] = name;
        DEBUGLOG("Added new node ext %d(int %d)\n", name, newNode);
        return newNode;
    }

    bool FlareGraph::setNodeArea(int name, fea_t start, fea_t end) {
        if (name < 0) {
            LOG("Bad name: %d", name);
            return false;
        }
        int in1 = mapNode(name);
        if (in1 < 0) {
            return false;
        }
        blocks[in1].start = start;
        blocks[in1].end = end;
        return true;
    }

    bool FlareGraph::addEdge(int n1, int n2) {
        int in1 = mapNode(n1);
        int in2 = mapNode(n2);

        if ((in1 < 0) || (in2 < 0)) {
            // bad names
            LOG("Bad edge names %d %d\n", n1, n2);
            return false;
        }
        if (std::find(succs[in1].begin(), succs[in1].end(), in2) != succs[in1].end()) {
            // edge already in in1's successor set
            LOG("Edge %d -> %d already exists\n", n1, n2);
            return false;
        }
        if (std::find(preds[in2].begin(), preds[in2].end(), in1) != preds[in2].end()) {
            LOG("Edge %d -> %d already exists\n", n1, n2);
            // edge already in in2's predecssor set
            return false;
        }
        DEBUGLOG("Added edge %d -> %d\n", n1, n2);
        succs[in1].push_back(in2);
        preds[in2].push_back(in1);
        ++edgeCount;
        return true;
    }

    bool FlareGraph::colorNode(int n1, const char* color) {
        int in1 = mapNode(n1);
        if (in1 < 0) {
            return false;
        }
        nodeColors[in1] = std::string(color);
        return true;
    }

    bool FlareGraph::selectNodes(intset_t &nodes) {
        int rnode = getRootNode();
        for (auto it = nodes.begin(); it != nodes.end(); ++it) {
            // TODO: do i need to filter selecting the root?
            if (rnode == *it) {
                continue;
            }
            addNodeProp(*it, NodeProperty::Selected);
        }
        return true;
    }

    bool FlareGraph::clearNodeProp(int n1, NodeProperty prop) {
        int in1 = mapNode(n1);

        if (in1 < 0) {
            // bad names
            return false;
        }
        nodeProps[in1].erase(prop);
        if (prop == NodeProperty::Root) {
            rootNode = -1;
        } else if (prop == NodeProperty::Sentry) {
#if 0
            // orange TODO
            auto it = sentryNodes.find(in1);
            if (it != sentryNodes.end()) {
                sentryNodes.erase(it);
            }
#endif 
        }
        return true;
    }

    bool FlareGraph::addNodeProp(int n1, NodeProperty prop) {
        int in1 = mapNode(n1);

        if (in1 < 0) {
            // bad names
            return false;
        }
        nodeProps[in1].insert(prop);
        if (prop == NodeProperty::Root) {
            rootNode = in1;
        } else if (prop == NodeProperty::Sentry) {
            sentryNodes.push_back(in1);
        }
        return true;
    }

    // Stores the current graph to a simplified graphviz dot format
    // Mostly just used for debugging/testing
    bool FlareGraph::storeGraphViz(std::ostream &os) {
        std::vector<std::string> nodes;
        std::vector<std::string> props;
        for (int n1 = 0; (unsigned int)n1 < succs.size(); ++n1) {
            for (int n2 = 0; n2 < nsucci(n1); ++n2) {
                std::ostringstream currEdge;
                currEdge << "    " << reverseMapNode(n1) << " -> " << reverseMapNode(succi(n1, n2)) << "\n";
                nodes.push_back(currEdge.str());
            }
        }
        for (nodepropiter_t nodeit = nodeProps.begin(); nodeit != nodeProps.end(); ++nodeit) {
            for (propiter_t propit = nodeit->second.begin(); propit != nodeit->second.end(); ++propit) {
                std::ostringstream currProp;
                currProp << "    " << reverseMapNode(nodeit->first) << " [ style=\"filled\" fillcolor=\"";
                switch (*propit) {
                case NodeProperty::Selected:
                    currProp << NODE_COLOR_SELECTED;
                    break;
                case NodeProperty::Pruned:
                    currProp << NODE_COLOR_PRUNED;
                    break;
                case NodeProperty::Root:
                    currProp << NODE_COLOR_ROOT;
                    break;
                case NodeProperty::Sentry:
                    currProp << NODE_COLOR_SENTRY;
                    break;
                }
                currProp << "\"]\n";
                props.push_back(currProp.str());
            }
        }
        // now the raw node colors. not to be used concurrently with our overloaded node properties!
        for (auto it = nodeColors.begin(); it != nodeColors.end(); ++it) {
            std::ostringstream currProp;
            currProp << "    " << reverseMapNode(it->first) << " [ style=\"filled\" fillcolor=\"" << it->second << "\"]\n";
            props.push_back(currProp.str());
        }

        // now sort the lines to ensure deterministic output
        std::sort(nodes.begin(), nodes.end());
        std::sort(props.begin(), props.end());
        // write out file & graph location
        if (inputPath.size() != 0) {
            os << "// file " << inputPath << "\n";
        }
        // remember that the "rootNode" is the start for analysis, it likely
        //  isn't the function start, but at least we can track the ea we're
        //  analyzing
        if (functionEa != -1) {
            os << "// function " << std::hex << functionEa << "\n";
        }

        os << "digraph G {" << "\n";
        for (unsigned int i = 0; i < nodes.size(); ++i) {
            os << nodes[i];
        }
        for (unsigned int i = 0; i < props.size(); ++i) {
            os << props[i];
        }
        os << "}";
        return true;
    }

    // SUPER HACKY GRAPHVIZ PARSING
    // only very simplistic graphs supported (like those generated by this class)
    // we also abuse node colors to encode extra data. So yeah, don't use this
    // for general use -> actually link to graphviz. Only simple "<node1> -> <node2>"
    // lines and "<node1> [<node_color_property>]" lines are supported.
    bool FlareGraph::loadGraphViz(std::istream &in) {
        std::string line;
        int state = 0;
        while (std::getline(in, line)) {
            //first look for end of line comments
            std::size_t pos = line.find("//");
            if (pos != std::string::npos) {
                line = line.substr(0, pos);
            }
            boost::trim(line);
            DEBUGLOG("Examining input now: '%s'\n", line.c_str());
            if (state == 0) {
                if (line.size() == 0) {
                // pass
                } else if ((line.find("digraph") == 0) && (line.find("{") != std::string::npos)) {
                    DEBUGLOG("State 0->1 (graph and open brace)\n");
                    state = 1;
                } else {
                    DEBUGLOG("Unknown state0 line: '%s'\n", line.c_str());
                    return false;
                }
            } else if (state == 1) {
                if (line.size() == 0) {
                    // pass
                } else if (line.find("}") == 0) {
                    DEBUGLOG("State 2->3 (close brace)\n");
                    state = 2;
                } else if (line.find("->") != std::string::npos) {
                    DEBUGLOG("Edge definition\n");
                    std::vector<std::string> strs;
                    boost::split(strs, line, boost::is_any_of("\t "));
                    if ((strs.size() != 3) || (strs[1] != "->")) {
                        LOG("Unexpected edge format: %s\n", line.c_str());
                        for (unsigned int i = 0; i < strs.size(); ++i) {
                            LOG("strs[%d]: %s\n", i, strs[i].c_str());
                        }
                        return false;
                    } else {
                        int n1 = atoi(strs[0].c_str());
                        int n2 = atoi(strs[2].c_str());
                        DEBUGLOG("add %d -> %d edge\n", n1, n2);
                        addNode(n1);
                        addNode(n2);
                        addEdge(n1, n2);
                    }
                } else if (line.find("[") != std::string::npos) {
                    DEBUGLOG("Node property\n");
                    std::vector<std::string> strs;
                    boost::split(strs, line, boost::is_any_of("\t "));
                    int n1 = atoi(strs[0].c_str());
                    if (line.find(NODE_COLOR_SELECTED) != std::string::npos) {
                        DEBUGLOG("node selected: %d\n", n1);
                        addNodeProp(n1, NodeProperty::Selected);
                    } else if (line.find(NODE_COLOR_PRUNED) != std::string::npos) {
                        DEBUGLOG("node pruned: %d\n", n1);
                        addNodeProp(n1, NodeProperty::Pruned);
                    } else if (line.find(NODE_COLOR_ROOT) != std::string::npos) {
                        DEBUGLOG("node root: %d\n", n1);
                        addNodeProp(n1, NodeProperty::Root);
                    } else if (line.find(NODE_COLOR_SENTRY) != std::string::npos) {
                        DEBUGLOG("node sentry: %d\n", n1);
                        addNodeProp(n1, NodeProperty::Sentry);
                    } else {
                        DEBUGLOG("Unknown property line: '%s'\n", line.c_str());
                        return false;
                    }
                } else {
                    LOG("Unknown state1 line: '%s'\n", line.c_str());
                    return false;
                }
            } else {
                LOG("Unexpected state\n");
                return false;
            }
        }
        return true;
    }

    bool FlareGraph::uniqueReachable(intset_t &out) {
        std::set<int>::iterator it;
        intset_t reachableSet;
        bool ret = uniqueReachablei(reachableSet);
        if (!ret) {
            return false;
        }
        for (it = reachableSet.begin(); it != reachableSet.end(); ++it) {
            out.insert(reverseMapNode(*it));
        }
        return true;
    }

    // internal unique-reachable calculation. uses the current root node as the start.
    bool FlareGraph::uniqueReachablei(intset_t &out) {

        //1) Traverse graph, add each reachable node to the reachableSet vector.
        //  if we encounger a node already in the reachable group, ignore it since it's already processed)
        //  if we encounter a sentry node, ignore it & its children (already Processed)
        //  process until stack is emtpy
        //2) For every node in the reachableSet, check if it has any preds not in the graph, 
        //  which would mean it is reachable out of the newGroup, remove the node (i.e. add 
        //  to a toRemove vector),  do a traversal on its children, also adding them to the 
        //  toRemove vector (if not already present).
        //3) fill newGroup, which is all nodes in reachableSet that aren't in toRemove
        intset_t reachableSet;
        intset_t toPrune;
        int anode;
        bool ret = reachablei(reachableSet);
        if (!ret) {
            DEBUGLOG("reachablei failed\n");
            return false;
        }
        DEBUGLOG("Beginning pruning of %d reachable nodes\n", reachableSet.size());
        for (auto it = reachableSet.begin(); it != reachableSet.end(); ++it) {
            anode = *it;
            if (anode == rootNode) {
                // rootNode will likely have non-reachable preds, but need to keep it
                continue;
            }
            if (intsetHas(toPrune, anode)) {
                // we've already pruned this node & all it's reachable children. just continue
                continue;
            }
            bool prune = false;
            for (int j = 0; j < npredi(anode); ++j) {
                int pred = predi(anode, j);
                if (!intsetHas(reachableSet, pred)) {
                    // not reachable, so we need to prune anode current node and 
                    // all reachable children of anode
                    prune = true;
                    break;
                } else {
                    // nothing, we're good
                }
            }
            if (prune) {
                // do a traversal starting at this node, adding this node and all reachable 
                // children to the toPrune vector
                intdeque_t pendingAnalysis;
                pendingAnalysis.push_back(anode);
                while (pendingAnalysis.size() > 0) {
                    // pnode: node to prune
                    int pnode = pendingAnalysis.back();
                    pendingAnalysis.pop_back();
                    if (!intsetHas(reachableSet, pnode)) {
                        // the node isn't reachable... this shouldn't hit, right?
                        continue;
                    }
                    if (!intsetHas(toPrune, pnode)) {
                        // not already in toPrune, so add it
                        toPrune.insert(pnode);
                        // queue all children nodes for analysis
                        for (int i = 0; i < nsucci(pnode); ++i) {
                            int snode = succi(pnode, i);
                            DEBUGLOG("pruning pendingAnalysis.has(snode) %d items\n", pendingAnalysis.size());
                            if ((snode != rootNode) && (!intdequeHas(pendingAnalysis, snode))) {
                                pendingAnalysis.push_back(snode);
                            }
                        }
                    }
                }
            }
        }
        // okay, now build the newGroup vector: reachableSet - toPrune
        for (auto it = reachableSet.begin(); it != reachableSet.end(); ++it) {
            anode = *it;
            // if the node isn't in the toPrune set, add it to the output 
            if (toPrune.find(anode) == toPrune.end()) {
                out.insert(anode);
            }
        }
        return true;
    }

    // public wrapper around reachablei()
    bool FlareGraph::reachable(intset_t &out) {
        std::set<int>::iterator it;
        intset_t reachableSet;
        bool ret = reachablei(reachableSet);
        if (!ret) {
            return false;
        }
        for (it = reachableSet.begin(); it != reachableSet.end(); ++it) {
            out.insert(reverseMapNode(*it));
            DEBUGLOG("Adding reachable node %d\n", reverseMapNode(*it));
        }
        if (out.size() != reachableSet.size()) {
            LOG("Ruh roh: size mismatch %d vs %d\n", reachableSet.size(), out.size());
        }
        return true;
    }


    // internal reachable graph search. uses the current root node as the start
    bool FlareGraph::reachablei(intset_t &reachableSet) {
        intdeque_t pendingAnalysis;
        if (rootNode < 0) {
            LOG("No root node set\n");
            return false;
        }
        int anode = -1;
        int succ = -1;
        pendingAnalysis.push_back(rootNode);
        while (pendingAnalysis.size() > 0) {
            anode = pendingAnalysis.back();
            pendingAnalysis.pop_back();
            DEBUGLOG("Now examining %d\n", reverseMapNode(anode));
            if (intsetHas(reachableSet, anode)) {
                // nothing
                DEBUGLOG("Skipping already in reachableSet %d\n", reverseMapNode(anode));
                continue;
            } else {
                DEBUGLOG("pendingAnalysis.has(anode) %d items\n", pendingAnalysis.size());
                if (intdequeHas(pendingAnalysis, anode)) {
                    // nothing
                    DEBUGLOG("Skipping already in pendingAnalysis.has %d\n", reverseMapNode(anode));
                    continue;
                } else if (!intvectorHas(sentryNodes, anode)) {
                    // only add if it's not a sentry node
                    DEBUGLOG("Queueing reachable pendingAnalysis %d\n", reverseMapNode(anode));
                    reachableSet.insert(anode);
                    // push all children succcessors to the pendingAnalysis stack
                    for (int i = 0; i < nsucci(anode); ++i) {
                        succ = succi(anode, i);
                        DEBUGLOG("pendingAnalysis.has(succ) %d items\n", pendingAnalysis.size());
                        if (!intdequeHas(pendingAnalysis, succ)) {
                            DEBUGLOG("Queueing %d -> %d\n", reverseMapNode(anode), reverseMapNode(succ));
                            pendingAnalysis.push_back(succ);
                        }
                    }
                }
            }
        }
        DEBUGLOG("Root node: %d. Found %d reachable nodes\n", reverseMapNode(rootNode), reachableSet.size());
        if (reachableSet.size() == 0) {
            LOG("Ruh roh. Reachable is 0. That can't be right\n");
            return true;
        }
        return true;
    }

    // public immediate-dominator tree calculation.
    bool FlareGraph::idom(intintMap_t &outdom, int entry) {
        DEBUGLOG("Creating idom DomGraph with %d nodes\n", succs.size());
        DomGraph dgr(succs.size());
        for (int n1 = 0; (unsigned int)n1 < succs.size(); ++n1) {
            for (int n2 = 0; n2 < nsucci(n1); ++n2) {
                boost::add_edge(boost::vertex(n1, dgr), boost::vertex(succi(n1, n2), dgr), dgr);
            }
        }
        return idomHelper(dgr, outdom, entry);
    }

    // public immediate-post-dominator tree calculation.
    bool FlareGraph::ipdom(intintMap_t &outdom, int entry) {
        DomGraph dgr(succs.size());
        for (int n1 = 0; (unsigned int)n1 < succs.size(); ++n1) {
            for (int n2 = 0; n2 < nsucci(n1); ++n2) {
                //manually reverse the graph edges rather than boost's make_reverse
                boost::add_edge(boost::vertex(succi(n1, n2), dgr), boost::vertex(n1, dgr), dgr);
            }
        }
        return idomHelper(dgr, outdom, entry);
    }

    // actual implementation for the immediate-dominator calculation.
    // entry is the external name of the node to start the search at
    bool FlareGraph::idomHelper(DomGraph &dgr, intintMap_t &outdom, int entry) {
        int in1 = mapNode(entry);
        if (in1 < 0) {
            // bad names
            return false;
        }
        boost::graph_traits<DomGraph>::vertex_iterator uItr, uEnd;
        VertexVertexMap dominatorTree;
        boost::associative_property_map<VertexVertexMap> domTreePredMap(dominatorTree);
        lengauer_tarjan_dominator_tree(dgr, boost::vertex(in1, dgr), domTreePredMap);

        DEBUGLOG("Creating idom %d items\n", boost::num_vertices(dgr));

        for (boost::tie(uItr, uEnd) = boost::vertices(dgr); uItr != uEnd; ++uItr) {
            int n1 = reverseMapNode((int)(*uItr));
            if (dominatorTree.find(*uItr) == dominatorTree.end()) {
                DEBUGLOG("Adding res[%d] = null\n", n1);
                //outdom[reverseMapNode(*uItr)] = (std::numeric_limits<int>::max)();
                outdom[reverseMapNode((int)(*uItr))] = -1;
            } else {
                int n2 = reverseMapNode((int)(dominatorTree[(int)(*uItr)]));
                DEBUGLOG("Adding res[%d] = %d\n", n1, n2);
                outdom[n1] = n2;
            }
        }
        return true;
    }

    // gets the immediate dominator (or postdominator) tree by iterating over
    // all entry points (or terminals) and throwing out inconsistencies
    // The intintMap_t out contains public names of nodes (not internal)
    //    doipdom: true -> calculate the immediate post-dominator tree
    //    doipdom: false -> calculate the immediate dominator tree
    bool FlareGraph::getDomIntMap(intintMap_t &out, bool doipdom) {
        //first thing: look for terminal nodes to do ipdom analysis from
        bool ret;
        intvector_t terms;
        for (unsigned int i = 0; i < succs.size(); ++i) {
            if (doipdom) {
                if (nsucci(i) == 0) {
                    DEBUGLOG("Found term node: %d\n", reverseMapNode(i));
                    terms.push_back(i);
                }
            } else {
                if (npredi(i) == 0) {
                    DEBUGLOG("Found term node: %d\n", reverseMapNode(i));
                    terms.push_back(i);
                }
            }
        }
        if (terms.size() == 0) {
            // TODO: is there something we can do here? choose deepest node in a dfs traversal?
            LOG("No terminal nodes. Likely an infinite function. Sorry - not yet implemented\n");
            return false;
        }
        DEBUGLOG("Found %d terminal nodes to examine\n", terms.size());
        int count = 0;
        // loop over each entry/terminal in terms to calculate the idom/ipdom tree
        //  then check for inconsistencies between runs. Throw out the inconsistencies.
        for (unsigned int i = 0; i < terms.size(); ++i) {
            int extterm = reverseMapNode(terms[i]);
            //remember that curripdom will have external names after ipdom() completes
            DEBUGLOG("%d running ipdom: %d\n", i, extterm);
            if (count == 0) {
                //first entry, so add everyting to out
                if (doipdom) {
                    ret = ipdom(out, extterm);
                } else {
                    ret = idom(out, extterm);
                }
                if (!ret) {
                    LOG("ipdom failed for node %d\n", extterm);
                    continue;
                }
            } else {
                // store to a local intintMap_t and then compare pair-wise
                intintMap_t curripdom;
                if (doipdom) {
                    ret = ipdom(curripdom, extterm);
                } else {
                    ret = idom(curripdom, extterm);
                }
                if (!ret) {
                    LOG("ipdom failed for node %d\n", extterm);
                    continue;
                }
                for (auto mit = curripdom.begin(); mit != curripdom.end(); ++mit) {
                    // mit->first (index): node that has a valid ipdominator
                    // mit->second: the ipdominator for the given node
                    // dit->second: the previously discovered ipdominator for the same node
                    auto dit = out.find(mit->first);
                    if (dit == out.end()) {
                        // not in the  out, so leave it out -> is this possible??
                        DEBUGLOG("WARNING: Skipping curripdom ipdom[%d] = %d\n", extterm, mit->first, mit->second);
                    } else if (mit->second == dit->second) {
                        // they're in agreement. great
                        //LOG("Agreement: %d) ipdom[%d] = %d\n", extterm, mit->first, mit->second);
                    } else if (mit->second != dit->second) {
                        if ((mit->second == IPDOM_CONFLICT) || (dit->second == IPDOM_CONFLICT)) {
                            //already in conflict, so nothing to do
                            DEBUGLOG("Ignoring ipdom already in conflict: %d) ipdom[%d] = %d  & ipdom[%d] = %d\n", 
                                    extterm, mit->first, mit->second, mit->first, dit->second
                            );
                            continue;
                        } else if ((mit->second != IPDOM_UNKNOWN) && (dit->second == IPDOM_UNKNOWN)) {
                            // newly discovered value overrides IPDOM_UNKNOWN
                            out[mit->first] = mit->second;
                            DEBUGLOG("Setting value:   %d) ipdom[%d] = %d\n", extterm, mit->first, mit->second);
                        } else if ((mit->second == IPDOM_UNKNOWN) && (dit->second != IPDOM_UNKNOWN)) {
                            // ignore this, already have a value, mit->second doesn't help
                            DEBUGLOG("Ignoring value:   %d) ipdom[%d] = %d\n", extterm, mit->first, mit->second);
                        } else {
                            // conflict detected, so mark it
                            DEBUGLOG("Setting conflict %d) ipdom[%d] = %d  & ipdom[%d] = %d\n", extterm, mit->first, mit->second, mit->first, dit->second);
                            out[mit->first] = IPDOM_CONFLICT;
                        }
                    }
                }
            }
            ++count;
        }
        return true;
    }

    // the index of out is the ipdom, the value is the set of nodes
    // post-dominated by its ipdom (index)
    // nodes that don't have an ipdom won't appear in this data structure
    // doipdom -> if true calculate postdominator map. if false, calculate dominator map
    bool FlareGraph::getipdomMap(intsetMap_t &out, bool doipdom) {
        intintMap_t runningIpdom ; 
        bool ret = getDomIntMap(runningIpdom, doipdom);
        if (!ret) {
            return false;
        }
        // at this point, we have the ipdom tree consistent across all terminals
        // convert runningIpdom to the intsetMap_t out
        for (auto it = runningIpdom.begin(); it != runningIpdom.end(); ++it) {
            out[ it->second ].insert(it->first);
        }
        return true;
    }

    bool FlareGraph::getpdomGroup(intset_t &outset, int entry) {
        // TODO: do i need to run unique reachable first? seems like no
        int in1 = mapNode(entry);
        if (in1 < 0) {
            return false;
        }
        //first thing: look for terminal nodes to do ipdom analysis from
        intvector_t terms;
        for (unsigned int i = 0; i < succs.size(); ++i) {
            if (nsucci(i) == 0) {
                DEBUGLOG("Found term node: %d\n", reverseMapNode(i));
                terms.push_back(i);
            }
        }
        if (terms.size() == 0) {
            // TODO: choose deepest node in a dfs traversal?
            LOG("No terminal nodes. Likely an infinite function. Sorry - not yet implemented\n");
            return false;
        }
        // remember that goodTerm is external name
        int goodTerm = -1;
        // remember that goodipdom is external name
        int goodipdom = -1;
        DEBUGLOG("Found %d terminal nodes to examine\n", terms.size());
        for (unsigned int i = 0; i < terms.size(); ++i) {
            intintMap_t curripdom;
            int extterm = reverseMapNode(terms[i]);
            //remember that curripdom will have external names after ipdom() completes
            DEBUGLOG("%d running ipdom: %d\n", i, extterm);
            bool ret = ipdom(curripdom, extterm);
            if (!ret) {
                DEBUGLOG("ipdom failed for node %d\n", extterm);
                continue;
            }

            if (goodipdom < 0) {
                goodipdom = curripdom[entry];
                goodTerm = extterm;
                DEBUGLOG("Setting initial goodipdom: %d %d\n", goodipdom, goodTerm);
            } else {
                if (goodipdom == curripdom[entry]) {
                    // looking good. confirmation among different terminators
                    DEBUGLOG("Match goodipdom: node[%d] = %d\n", goodTerm, goodipdom);
                } else {
                    LOG("Mismatch immediate dominators for different terminators: term[%d] = %d. term[%d] %d\n", 
                            goodTerm, goodipdom, extterm, curripdom[entry]
                    );
                    return false;
                }
            }
        }
        if ((goodTerm < 0) || (goodipdom < 0)) {
            LOG("Failed to find good terminator to perform post-dominator analysis. :(\n");
            return false;
        }
        DEBUGLOG("Found the good %d. ipdom[%d] = %d\n", goodTerm, entry, goodipdom);
        // now we can just do a traversal starting at entry, stopping at goodipdom (inclusive) to create the set of nodes to join
        intdeque_t pendingAnalysis;
        //queue the entry point node we're trying to collapse
        pendingAnalysis.push_back(in1);

        //add the good ipdom so we don't follow its successors in the traversal
        outset.insert(goodipdom);

        while (pendingAnalysis.size() > 0) {
            DEBUGLOG("Current queue len: %d\n", pendingAnalysis.size());
            int pnode = pendingAnalysis.back();
            pendingAnalysis.pop_back();
            int opnode = reverseMapNode(pnode);
            DEBUGLOG("Got pending analysis node ext %d (int %d)\n", opnode, pnode);
            if (intsetHas(outset, opnode)) {
                // already in the output set, don't follow successors
                DEBUGLOG("outset already has opnode %d\n", opnode);
                continue;
            }
            // add to output
            outset.insert(opnode);
            // queue all children nodes for analysis
            for (int k = 0; k < nsucci(pnode); ++k) {
                int snode = succi(pnode, k);
                int osnode = reverseMapNode(snode);
                if ((!intsetHas(outset, osnode) && (!intdequeHas(pendingAnalysis, snode)))) {
                    DEBUGLOG("Queueing successor ext %d (int %d)\n", osnode, snode);
                    pendingAnalysis.push_back(snode);
                }
            }
        }
        return true;
    }

    // top-level function to find isolated subgraphs of the current graph with
    // the given min/max parameters.
    bool FlareGraph::findSimpleSubGraphs(subgraphVec_t &out, unsigned int minNodeCount, unsigned int maxNodePercentage) {
        intintMap_t ipdomMap;
        intintPairSet_t candidates;
        bool ret = getDomIntMap(ipdomMap, true);
        if (!ret) {
            LOG("Failed get ipdom map\n");
            return false;
        }

        intintMap_t idomMap;
        ret = getDomIntMap(idomMap, false);
        if (!ret) {
            LOG("Failed get idom map\n");
            return false;
        }

        DEBUGEXEC({
            DEBUGLOG("Got ipdom map with %d entries\n", ipdomMap.size());
            for (auto pit = ipdomMap.begin(); pit != ipdomMap.end(); ++pit) {
                DEBUGLOG("ipdom[%d] = %d\n", pit->first, pit->second);
            }
            DEBUGLOG("Got idom map with %d entries\n", idomMap.size());
            for (auto dit = idomMap.begin(); dit != idomMap.end(); ++dit) {
                DEBUGLOG("idom[%d] = %d\n", dit->first, dit->second);
            }
        });

        for (auto pit = ipdomMap.begin(); pit != ipdomMap.end(); ++pit) {
            auto dit = idomMap.find(pit->second);
            if (dit == idomMap.end()) {
                // not an error
                continue;
            }
            if ((dit->second == pit->first) && (dit->first == pit->second)) {
                DEBUGLOG("Matching idom/ipdom  nodes %d <=> %d\n", pit->first, dit->first);
                candidates.insert(std::make_pair(pit->first, dit->first));
            }
        }
        return verifySimpleSubGraphs(out, candidates, minNodeCount, maxNodePercentage);
    }

    //given a set of pairs of <dom,pdom>, verifies if they actually 
    bool FlareGraph::verifySimpleSubGraphs(subgraphVec_t &out, intintPairSet_t &candidates, unsigned int minNodeCount, unsigned int maxNodePercentage) {
        for (auto it = candidates.begin(); it != candidates.end(); ++it) {
            int n1 = it->first;
            int n2 = it->second;
            subgraph_t res(n1, n2);
            bool ret = isSimpleSubGraph(n1, n2, res, minNodeCount, maxNodePercentage);
            if (ret) {
                DEBUGEXEC({
                    DEBUGLOG("Found simple graph <%d,%d>: %d nodes\n", n1, n2, res.nodes.size());
                    for (auto nit = res.nodes.begin(); nit != res.nodes.end(); ++nit) {
                        DEBUGLOG("  %d\n", *nit);
                    }
                });
                out.push_back(res);
            } else {
                DEBUGLOG("Skipping simple graph <%d,%d>\n", n1, n2);
            }
        }
        return true; 
    }

    // given dominator n1 & postdominator n2, tries to get nodes  of subgraph
    // and adds nodes to outset. returns false if this isn't a good subgraph
    //   1) If there are 4 or fewer nodes
    //   2) If a consistency check fails due to reaching an unexpected terminal node
    bool FlareGraph::isSimpleSubGraph(int n1, int n2, subgraph_t &outgraph, unsigned int minNodeCount, unsigned int maxNodePercentage) {
        DEBUGLOG("isSimpleSubGraph: %d %d\n", n1, n2);
        int in1 = mapNode(n1);
        if (in1 < 0) {
            outgraph.nodes.clear();
            return false;
        }
        int in2 = mapNode(n2);
        if (in2 < 0) {
            outgraph.nodes.clear();
            return false;
        }
  
        intdeque_t pendingAnalysis;
        //queue the entry point node we're trying to collapse
        pendingAnalysis.push_back(in1);

        //add the good ipdom so we don't follow its successors in the graph traversal
        outgraph.nodes.insert(n2);

        while (pendingAnalysis.size() > 0) {
            int pnode = pendingAnalysis.back();
            pendingAnalysis.pop_back();
            int opnode = reverseMapNode(pnode);
            DEBUGLOG("Examining opnode %d\n", opnode);
            if (intsetHas(outgraph.nodes, opnode)) {
                // already in the output set, don't follow successors
                continue;
            }
            if ((opnode != in2) && (nsucc(opnode) == 0)) {
                // not a problem. means we encountered found another termina
                DEBUGLOG("Found unexpected terminal during %d:( %08x ) %d:( %08x ) verification: %d:( %08x )\n", n1, getNodeAreaStart(n1), n2, getNodeAreaStart(n2), opnode, getNodeAreaStart(opnode));
                outgraph.nodes.clear();
                return false;
            }
            // add to output
            outgraph.nodes.insert(opnode);
            // queue all children nodes for analysis
            for (int k = 0; k < nsucci(pnode); ++k) {
                int snode = succi(pnode, k);
                int osnode = reverseMapNode(snode);
                if ((!intsetHas(outgraph.nodes, osnode) && (!intdequeHas(pendingAnalysis, snode)))) {
                    DEBUGLOG("Queueing successor ext %d -> %d\n", opnode, osnode);
                    pendingAnalysis.push_back(snode);
                }
            }
        }
        if (outgraph.nodes.size() < minNodeCount) {
            DEBUGLOG("Subgraph too small <%d,%d>: %d nodes > %d\n", n1, n2, outgraph.nodes.size(), minNodeCount);
            outgraph.nodes.clear();
            return false;
        }
        double coverPercent = double(outgraph.nodes.size())/getNodeCount();
        unsigned int coverPercentInt = unsigned int(coverPercent*100.0);
        if (coverPercentInt > maxNodePercentage) {
            DEBUGLOG("Subgraph too big <%d,%d>: %d nodes. %d percent > %d\n", n1, n2, outgraph.nodes.size(), coverPercentInt, maxNodePercentage);
            outgraph.nodes.clear();
            return false;
        }
        bool toret = true;
        //make sure that only the dominator is has predecessors not in the candiate subgraph
        for (auto it = outgraph.nodes.begin(); it != outgraph.nodes.end(); ++it) {
            if (!toret) {
                break;
            }
            if (*it == n1) {
                continue;
            }
            for (int j = 0; j < npred(*it); ++j) {
                int pnode = pred(*it, j);
                if (!intsetHas(outgraph.nodes, pnode)) {
                    // predecessor not in 
                    DEBUGLOG("Subgraph has invalide predecessor <%d,%d>: %d->%d\n", n1, n2, pnode, *it);
                    toret = false;
                    break;
                }
            }
        }
        if(!toret) {
            outgraph.nodes.clear();
            return false;
        }
        return true;
    }
}
