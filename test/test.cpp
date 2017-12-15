// test.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include "FlareGraph.hpp"
#include <string>
#include <sstream>

#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#include "catch.hpp"

std::string test1str = 
"digraph G {\n"
"    0 -> 1\n"
"    0 -> 2\n"
"    1 -> 3\n"
"    2 -> 3\n"
"    0 [ style=\"filled\" fillcolor=\"green\"]\n"
"    1 [ style=\"filled\" fillcolor=\"lightskyblue\"]\n"
"    2 [ style=\"filled\" fillcolor=\"gray\"]\n"
"    3 [ style=\"filled\" fillcolor=\"firebrick1\"]\n"
"}";

std::string g_test2 = R"(// 00209820
digraph G {
    0 -> 1
    0 -> 2
    2 -> 3
    2 -> 7
    3 -> 4
    3 -> 7
    4 -> 5
    4 -> 8
    5 -> 6
    6 -> 7
    8 -> 6
    8 -> 9
    4 [ style="filled" fillcolor="green"]
})";

std::string g_test2_expect = R"(digraph G {
    0 -> 1
    0 -> 2
    2 -> 3
    2 -> 7
    3 -> 4
    3 -> 7
    4 -> 5
    4 -> 8
    5 -> 6
    6 -> 7
    8 -> 6
    8 -> 9
    4 [ style="filled" fillcolor="green"]
})";

unsigned int Factorial(unsigned int number) {
    return number <= 1 ? number : Factorial(number - 1)*number;
}

TEST_CASE("Factorials are computed", "[factorial]") {
    REQUIRE(Factorial(1) == 1);
    REQUIRE(Factorial(2) == 2);
    REQUIRE(Factorial(3) == 6);
    REQUIRE(Factorial(10) == 3628800);
}

TEST_CASE("Basic graph creation") {
    Flare::FlareGraph fg;

    REQUIRE(fg.getNodeCount() == 0);
    fg.addNode(0);
    fg.addNode(1);
    fg.addNode(2);
    fg.addNode(3);
    REQUIRE(fg.getNodeCount() == 4);
    fg.addEdge(0, 1);
    fg.addEdge(0, 2);
    fg.addEdge(1, 3);
    fg.addEdge(2, 3);
    fg.addNodeProp(0, Flare::NodeProperty::Root);
    fg.addNodeProp(1, Flare::NodeProperty::Selected);
    fg.addNodeProp(2, Flare::NodeProperty::Pruned);
    fg.addNodeProp(3, Flare::NodeProperty::Sentry);
    REQUIRE(fg.hasSucc(0, 1));
    REQUIRE(fg.hasPred(0, 1));
    REQUIRE(fg.hasSucc(0, 2));
    REQUIRE(fg.hasPred(0, 2));
    REQUIRE(fg.hasSucc(1, 3));
    REQUIRE(fg.hasPred(1, 3));
    REQUIRE(fg.hasSucc(2, 3));
    REQUIRE(fg.hasPred(2, 3));
    std::ostringstream ostringstr;
    fg.storeGraphViz(ostringstr);
    
#if 0
    FILE *outf = fopen("t1.gv", "w");
    std::string outs = ostringstr.str();
    fwrite(outs.c_str(), 1, outs.size(), outf);
    fclose(outf);
#endif

    REQUIRE(ostringstr.str() == test1str);
}

TEST_CASE("Reacbale Test") {
    Flare::FlareGraph fg;

    fg.addNode(0);
    fg.addNode(1);
    fg.addNode(2);
    fg.addNode(3);
    fg.addNode(4);
    fg.addNode(5);
    fg.addNode(6);
    fg.addEdge(0, 1);
    fg.addEdge(0, 2);
    fg.addEdge(1, 6);
    fg.addEdge(2, 3);
    fg.addEdge(2, 4);
    fg.addEdge(3, 5);
    fg.addEdge(4, 5);
    fg.addEdge(5, 6);
    fg.addNodeProp(2, Flare::NodeProperty::Root);

    Flare::intset_t out1;
    bool ret = fg.reachable(out1);
    //fprintf(stderr, "Got %d reachable\n", out1.size());
    Flare::intset_t expect1 = { 2, 3, 4, 5, 6 };
    REQUIRE(out1 == expect1);

    Flare::intset_t out2;
    ret = fg.uniqueReachable(out2);
    //fprintf(stderr, "Got %d reachable\n", out2.size());
    Flare::intset_t expect2 = { 2, 3, 4, 5};
    REQUIRE(out2 == expect2);


    fg.clearNodeProp(2, Flare::NodeProperty::Root);
    fg.addNodeProp(0, Flare::NodeProperty::Root);
    Flare::intset_t out3;
    ret = fg.reachable(out3);
    //fprintf(stderr, "Got %d reachable\n", out3.size());
    Flare::intset_t expect3 = { 0, 1, 2, 3, 4, 5, 6 };
    REQUIRE(out3 == expect3);

}

TEST_CASE("GraphViz load") {
    Flare::FlareGraph fg;
    std::istringstream istream(g_test2);
    fg.loadGraphViz(istream);
    std::ostringstream ostringstr;
    fg.storeGraphViz(ostringstr);
    //printf("Got output: '%s'\n", ostringstr.str().c_str());
    REQUIRE(ostringstr.str().c_str() == g_test2_expect);

}

TEST_CASE("DominatorCorrectness") {
    // from boost test
    // Tarjan's paper
    Flare::FlareGraph fg;
    //printf("Starting dominator test\n");
    for (int i = 0; i < 13; ++i) {
        fg.addNode(i);
    }
    // Tarjan's paper
    fg.addEdge(0, 1);
    fg.addEdge(0, 2);
    fg.addEdge(0, 3);
    fg.addEdge(1, 4);
    fg.addEdge(2, 1);
    fg.addEdge(2, 4);
    fg.addEdge(2, 5);
    fg.addEdge(3, 6);
    fg.addEdge(3, 7);
    fg.addEdge(4, 12);
    fg.addEdge(5, 8);
    fg.addEdge(6, 9);
    fg.addEdge(7, 9);
    fg.addEdge(7, 10);
    fg.addEdge(8, 5);
    fg.addEdge(8, 11);
    fg.addEdge(9, 11);
    fg.addEdge(10, 9);
    fg.addEdge(11, 0);
    fg.addEdge(11, 9);
    fg.addEdge(12, 8);
    Flare::intintMap_t correctIdoms;
    correctIdoms[0] = -1;
    correctIdoms[1] = 0; //1 
    correctIdoms[2] = 0; //2
    correctIdoms[3] = 0; //3
    correctIdoms[4] = 0; //4
    correctIdoms[5] = 0; //5
    correctIdoms[6] = 3; //6
    correctIdoms[7] = 3; //7
    correctIdoms[8] = 0; //8
    correctIdoms[9] = 0; //9
    correctIdoms[10] = 7; //10
    correctIdoms[11] = 0; //11
    correctIdoms[12] = 4; //12
    Flare::intintMap_t res;
    bool ret = fg.idom(res, 0);
    REQUIRE(ret == true);
    //for (auto it = correctIdoms.begin(); it != correctIdoms.end(); ++it) {
    //    printf("correctIdoms[%d] = %d\n", it->first, it->second);
    //}
    //printf("res size: %d\n", res.size());
    //for (auto it = res.begin(); it != res.end(); ++it) {
    //    printf("res[%d] = %d\n", it->first, it->second);
    //}
    REQUIRE(res == correctIdoms);


}



struct DominatorCorrectnessTestSet
{
  typedef std::pair<int, int> edge;

  int numOfVertices;
  int start;
  std::vector<edge> edges;
  std::vector<int> correctIdoms;
};


TEST_CASE("DominatorCorrectness2") {
  typedef DominatorCorrectnessTestSet::edge edge;

  DominatorCorrectnessTestSet testSet[7];

  // Tarjan's paper
  testSet[0].numOfVertices = 13;
  testSet[0].start = 0,
  testSet[0].edges.push_back(edge(0, 1));
  testSet[0].edges.push_back(edge(0, 2));
  testSet[0].edges.push_back(edge(0, 3));
  testSet[0].edges.push_back(edge(1, 4));
  testSet[0].edges.push_back(edge(2, 1));
  testSet[0].edges.push_back(edge(2, 4));
  testSet[0].edges.push_back(edge(2, 5));
  testSet[0].edges.push_back(edge(3, 6));
  testSet[0].edges.push_back(edge(3, 7));
  testSet[0].edges.push_back(edge(4, 12));
  testSet[0].edges.push_back(edge(5, 8));
  testSet[0].edges.push_back(edge(6, 9));
  testSet[0].edges.push_back(edge(7, 9));
  testSet[0].edges.push_back(edge(7, 10));
  testSet[0].edges.push_back(edge(8, 5));
  testSet[0].edges.push_back(edge(8, 11));
  testSet[0].edges.push_back(edge(9, 11));
  testSet[0].edges.push_back(edge(10, 9));
  testSet[0].edges.push_back(edge(11, 0));
  testSet[0].edges.push_back(edge(11, 9));
  testSet[0].edges.push_back(edge(12, 8));
  testSet[0].correctIdoms.push_back(-1);
  testSet[0].correctIdoms.push_back(0);
  testSet[0].correctIdoms.push_back(0);
  testSet[0].correctIdoms.push_back(0);
  testSet[0].correctIdoms.push_back(0);
  testSet[0].correctIdoms.push_back(0);
  testSet[0].correctIdoms.push_back(3);
  testSet[0].correctIdoms.push_back(3);
  testSet[0].correctIdoms.push_back(0);
  testSet[0].correctIdoms.push_back(0);
  testSet[0].correctIdoms.push_back(7);
  testSet[0].correctIdoms.push_back(0);
  testSet[0].correctIdoms.push_back(4);

  // Appel. p441. figure 19.4
  testSet[1].numOfVertices = 7;
  testSet[1].start = 0,
  testSet[1].edges.push_back(edge(0, 1));
  testSet[1].edges.push_back(edge(1, 2));
  testSet[1].edges.push_back(edge(1, 3));
  testSet[1].edges.push_back(edge(2, 4));
  testSet[1].edges.push_back(edge(2, 5));
  testSet[1].edges.push_back(edge(4, 6));
  testSet[1].edges.push_back(edge(5, 6));
  testSet[1].edges.push_back(edge(6, 1));
  testSet[1].correctIdoms.push_back(-1);
  testSet[1].correctIdoms.push_back(0);
  testSet[1].correctIdoms.push_back(1);
  testSet[1].correctIdoms.push_back(1);
  testSet[1].correctIdoms.push_back(2);
  testSet[1].correctIdoms.push_back(2);
  testSet[1].correctIdoms.push_back(2);

  // Appel. p449. figure 19.8
  testSet[2].numOfVertices = 13,
  testSet[2].start = 0,
  testSet[2].edges.push_back(edge(0, 1));
  testSet[2].edges.push_back(edge(0, 2));
  testSet[2].edges.push_back(edge(1, 3));
  testSet[2].edges.push_back(edge(1, 6));
  testSet[2].edges.push_back(edge(2, 4));
  testSet[2].edges.push_back(edge(2, 7));
  testSet[2].edges.push_back(edge(3, 5));
  testSet[2].edges.push_back(edge(3, 6));
  testSet[2].edges.push_back(edge(4, 7));
  testSet[2].edges.push_back(edge(4, 2));
  testSet[2].edges.push_back(edge(5, 8));
  testSet[2].edges.push_back(edge(5, 10));
  testSet[2].edges.push_back(edge(6, 9));
  testSet[2].edges.push_back(edge(7, 12));
  testSet[2].edges.push_back(edge(8, 11));
  testSet[2].edges.push_back(edge(9, 8));
  testSet[2].edges.push_back(edge(10, 11));
  testSet[2].edges.push_back(edge(11, 1));
  testSet[2].edges.push_back(edge(11, 12));
  testSet[2].correctIdoms.push_back(-1);
  testSet[2].correctIdoms.push_back(0);
  testSet[2].correctIdoms.push_back(0);
  testSet[2].correctIdoms.push_back(1);
  testSet[2].correctIdoms.push_back(2);
  testSet[2].correctIdoms.push_back(3);
  testSet[2].correctIdoms.push_back(1);
  testSet[2].correctIdoms.push_back(2);
  testSet[2].correctIdoms.push_back(1);
  testSet[2].correctIdoms.push_back(6);
  testSet[2].correctIdoms.push_back(5);
  testSet[2].correctIdoms.push_back(1);
  testSet[2].correctIdoms.push_back(0);

  testSet[3].numOfVertices = 8,
  testSet[3].start = 0,
  testSet[3].edges.push_back(edge(0, 1));
  testSet[3].edges.push_back(edge(1, 2));
  testSet[3].edges.push_back(edge(1, 3));
  testSet[3].edges.push_back(edge(2, 7));
  testSet[3].edges.push_back(edge(3, 4));
  testSet[3].edges.push_back(edge(4, 5));
  testSet[3].edges.push_back(edge(4, 6));
  testSet[3].edges.push_back(edge(5, 7));
  testSet[3].edges.push_back(edge(6, 4));
  testSet[3].correctIdoms.push_back(-1);
  testSet[3].correctIdoms.push_back(0);
  testSet[3].correctIdoms.push_back(1);
  testSet[3].correctIdoms.push_back(1);
  testSet[3].correctIdoms.push_back(3);
  testSet[3].correctIdoms.push_back(4);
  testSet[3].correctIdoms.push_back(4);
  testSet[3].correctIdoms.push_back(1);
  
  // Muchnick. p256. figure 8.21
  testSet[4].numOfVertices = 8,
  testSet[4].start = 0,
  testSet[4].edges.push_back(edge(0, 1));
  testSet[4].edges.push_back(edge(1, 2));
  testSet[4].edges.push_back(edge(2, 3));
  testSet[4].edges.push_back(edge(2, 4));
  testSet[4].edges.push_back(edge(3, 2));
  testSet[4].edges.push_back(edge(4, 5));
  testSet[4].edges.push_back(edge(4, 6));
  testSet[4].edges.push_back(edge(5, 7));
  testSet[4].edges.push_back(edge(6, 7));
  testSet[4].correctIdoms.push_back(-1);
  testSet[4].correctIdoms.push_back(0);
  testSet[4].correctIdoms.push_back(1);
  testSet[4].correctIdoms.push_back(2);
  testSet[4].correctIdoms.push_back(2);
  testSet[4].correctIdoms.push_back(4);
  testSet[4].correctIdoms.push_back(4);
  testSet[4].correctIdoms.push_back(4);

  // Muchnick. p253. figure 8.18
  testSet[5].numOfVertices = 8,
  testSet[5].start = 0,
  testSet[5].edges.push_back(edge(0, 1));
  testSet[5].edges.push_back(edge(0, 2));
  testSet[5].edges.push_back(edge(1, 6));
  testSet[5].edges.push_back(edge(2, 3));
  testSet[5].edges.push_back(edge(2, 4));
  testSet[5].edges.push_back(edge(3, 7));
  testSet[5].edges.push_back(edge(5, 7));
  testSet[5].edges.push_back(edge(6, 7));
  testSet[5].correctIdoms.push_back(-1);
  testSet[5].correctIdoms.push_back(0);
  testSet[5].correctIdoms.push_back(0);
  testSet[5].correctIdoms.push_back(2);
  testSet[5].correctIdoms.push_back(2);
  testSet[5].correctIdoms.push_back(-1);
  testSet[5].correctIdoms.push_back(1);
  testSet[5].correctIdoms.push_back(0);

  // Cytron's paper, fig. 9
  testSet[6].numOfVertices = 14,
  testSet[6].start = 0,
  testSet[6].edges.push_back(edge(0, 1));
  testSet[6].edges.push_back(edge(0, 13));
  testSet[6].edges.push_back(edge(1, 2));
  testSet[6].edges.push_back(edge(2, 3));
  testSet[6].edges.push_back(edge(2, 7));
  testSet[6].edges.push_back(edge(3, 4));
  testSet[6].edges.push_back(edge(3, 5));
  testSet[6].edges.push_back(edge(4, 6));
  testSet[6].edges.push_back(edge(5, 6));
  testSet[6].edges.push_back(edge(6, 8));
  testSet[6].edges.push_back(edge(7, 8));
  testSet[6].edges.push_back(edge(8, 9));
  testSet[6].edges.push_back(edge(9, 10));
  testSet[6].edges.push_back(edge(9, 11));
  testSet[6].edges.push_back(edge(10, 11));
  testSet[6].edges.push_back(edge(11, 9));
  testSet[6].edges.push_back(edge(11, 12));
  testSet[6].edges.push_back(edge(12, 2));
  testSet[6].edges.push_back(edge(12, 13));
  testSet[6].correctIdoms.push_back(-1);
  testSet[6].correctIdoms.push_back(0);
  testSet[6].correctIdoms.push_back(1);
  testSet[6].correctIdoms.push_back(2);
  testSet[6].correctIdoms.push_back(3);
  testSet[6].correctIdoms.push_back(3);
  testSet[6].correctIdoms.push_back(3);
  testSet[6].correctIdoms.push_back(2);
  testSet[6].correctIdoms.push_back(2);
  testSet[6].correctIdoms.push_back(8);
  testSet[6].correctIdoms.push_back(9);
  testSet[6].correctIdoms.push_back(9);
  testSet[6].correctIdoms.push_back(11);
  testSet[6].correctIdoms.push_back(0);

  for (size_t i = 0; i < sizeof(testSet)/sizeof(testSet[0]); ++i) {
    const int numOfVertices = testSet[i].numOfVertices;
    Flare::FlareGraph fg;
    //printf("Starting dominator test\n");
    for (int j = 0; j < numOfVertices; ++j) {
        fg.addNode(j);
    }
    for (unsigned int j = 0; j < testSet[i].edges.size(); ++j) {
        fg.addEdge(testSet[i].edges[j].first, testSet[i].edges[j].second);
    }

    Flare::intintMap_t res;
    bool ret = fg.idom(res, testSet[i].start);
    REQUIRE(ret == true);
    REQUIRE(res.size() == testSet[i].correctIdoms.size());
    for (unsigned int j = 0; j < testSet[i].correctIdoms.size(); ++j) {
        REQUIRE(testSet[i].correctIdoms[j] == res[j]);
    }
#if 0
    for (auto it = correctIdoms.begin(); it != correctIdoms.end(); ++it) {
        printf("correctIdoms[%d] = %d\n", it->first, it->second);
    }
    printf("res size: %d\n", res.size());
    for (auto it = res.begin(); it != res.end(); ++it) {
        printf("res[%d] = %d\n", it->first, it->second);
    }
#endif
  }
}


TEST_CASE("PostDominator test") {
    typedef DominatorCorrectnessTestSet::edge edge;

    // http://web.cse.ohio-state.edu/~rountev.1/788/lectures/ControlFlowAnalysis.pdf
    // slide 13/14
    DominatorCorrectnessTestSet testSet[3];
    testSet[0].numOfVertices = 12;
    testSet[0].start = 11;
    testSet[0].edges.push_back(edge(0,1));
    testSet[0].edges.push_back(edge(1,2));
    testSet[0].edges.push_back(edge(1,3));
    testSet[0].edges.push_back(edge(2,3));
    testSet[0].edges.push_back(edge(3,4));
    testSet[0].edges.push_back(edge(4,3));
    testSet[0].edges.push_back(edge(4,6));
    testSet[0].edges.push_back(edge(4,5));
    testSet[0].edges.push_back(edge(5,7));
    testSet[0].edges.push_back(edge(6,7));
    testSet[0].edges.push_back(edge(7,4));
    testSet[0].edges.push_back(edge(7,8));
    testSet[0].edges.push_back(edge(8,3));
    testSet[0].edges.push_back(edge(8,9));
    testSet[0].edges.push_back(edge(8,10));
    testSet[0].edges.push_back(edge(9,1));
    testSet[0].edges.push_back(edge(10,7));
    testSet[0].edges.push_back(edge(10,11));
    testSet[0].correctIdoms.push_back(1);
    testSet[0].correctIdoms.push_back(3);
    testSet[0].correctIdoms.push_back(3);
    testSet[0].correctIdoms.push_back(4);
    testSet[0].correctIdoms.push_back(7);
    testSet[0].correctIdoms.push_back(7);
    testSet[0].correctIdoms.push_back(7);
    testSet[0].correctIdoms.push_back(8);
    testSet[0].correctIdoms.push_back(10);
    testSet[0].correctIdoms.push_back(1);
    testSet[0].correctIdoms.push_back(11);
    testSet[0].correctIdoms.push_back(-1);


    // http://pages.cs.wisc.edu/~fischer/cs701.f08/lectures/Lecture19.4up.pdf
    // slide 406
    testSet[1].numOfVertices = 6;
    testSet[1].start = 5;
    testSet[1].edges.push_back(edge(0,1));
    testSet[1].edges.push_back(edge(0,5));
    testSet[1].edges.push_back(edge(1,2));
    testSet[1].edges.push_back(edge(1,3));
    testSet[1].edges.push_back(edge(2,4));
    testSet[1].edges.push_back(edge(3,4));
    testSet[1].edges.push_back(edge(4,5));
    testSet[1].correctIdoms.push_back(5);
    testSet[1].correctIdoms.push_back(4);
    testSet[1].correctIdoms.push_back(4);
    testSet[1].correctIdoms.push_back(4);
    testSet[1].correctIdoms.push_back(5);
    testSet[1].correctIdoms.push_back(-1);

    // http://www.cs.kent.edu/~jmaletic/cs63901/lectures/StaticAnalysis.pdf
    testSet[2].numOfVertices = 7;
    testSet[2].start = 6;
    testSet[2].edges.push_back(edge(0,1));
    testSet[2].edges.push_back(edge(0,2));
    testSet[2].edges.push_back(edge(1,3));
    testSet[2].edges.push_back(edge(1,4));
    testSet[2].edges.push_back(edge(2,4));
    testSet[2].edges.push_back(edge(2,6));
    testSet[2].edges.push_back(edge(3,5));
    testSet[2].edges.push_back(edge(4,5));
    testSet[2].edges.push_back(edge(5,6));
    testSet[2].correctIdoms.push_back(6);
    testSet[2].correctIdoms.push_back(5);
    testSet[2].correctIdoms.push_back(6);
    testSet[2].correctIdoms.push_back(5);
    testSet[2].correctIdoms.push_back(5);
    testSet[2].correctIdoms.push_back(6);
    testSet[2].correctIdoms.push_back(-1);


    //printf("Starting postdominator test\n");

    for (size_t i = 0; i < sizeof(testSet)/sizeof(testSet[0]); ++i) {
        //for (unsigned int j = 0; j < testSet[i].correctIdoms.size(); ++j) {
        //    printf("correctIdoms[%d] = %d\n", j, testSet[i].correctIdoms[j]);
        //}
        int correctCount = 0;
        
        Flare::FlareGraph fg;

        for (int j = 0; j < testSet[i].numOfVertices; ++j) {
            fg.addNode(j);
        }
        for (unsigned int j = 0; j < testSet[i].edges.size(); ++j) {
            fg.addEdge(testSet[i].edges[j].first, testSet[i].edges[j].second);
        }
        std::ostringstream ostringstr;
        fg.storeGraphViz(ostringstr);
        //printf("%d postdom testgraph:\n%s\n", i, ostringstr.str());
        Flare::intintMap_t res;
        //printf("Starting ipdom\n");
        bool ret = fg.ipdom(res, testSet[i].start);
        REQUIRE(ret == true);
        REQUIRE(res.size() == testSet[i].correctIdoms.size());
        //for (auto it = res.begin(); it != res.end(); ++it) {
        //    printf("res[%d] = %d\n", it->first, it->second);
        //}
        for (unsigned int j = 0; j < testSet[i].correctIdoms.size(); ++j) {
            //REQUIRE(testSet[i].correctIdoms[j] == res[j]);
            if (testSet[i].correctIdoms[j] == res[j]) {
                ++correctCount;
            } else {
                printf("Incorrect res[%d]: %d vs %d\n", j, testSet[i].correctIdoms[j], res[j]);
            }
        }
        REQUIRE(correctCount == testSet[i].correctIdoms.size());
    }
}



TEST_CASE("PostDominatorGroupTest") {

std::string test1str = R"(
// file Z:\customer\2017\53142\smartidb_e3c5bd27d02977859acc6ce5570769a8.idb
// function 20a4c0
digraph G {
    0 -> 1
    0 -> 15
    1 -> 15
    1 -> 2
    10 -> 11
    10 -> 16
    11 -> 12
    12 -> 12
    12 -> 13
    13 -> 14
    13 -> 16
    14 -> 15
    2 -> 11
    2 -> 3
    3 -> 4
    4 -> 5
    4 -> 9
    5 -> 6
    5 -> 8
    6 -> 7
    6 -> 9
    7 -> 4
    7 -> 8
    8 -> 10
    9 -> 10
    4 [ style="filled" fillcolor="green"]
})";

std::string test1strsolution = R"(digraph G {
    0 -> 1
    0 -> 15
    1 -> 15
    1 -> 2
    10 -> 11
    10 -> 16
    11 -> 12
    12 -> 12
    12 -> 13
    13 -> 14
    13 -> 16
    14 -> 15
    2 -> 11
    2 -> 3
    3 -> 4
    4 -> 5
    4 -> 9
    5 -> 6
    5 -> 8
    6 -> 7
    6 -> 9
    7 -> 4
    7 -> 8
    8 -> 10
    9 -> 10
    10 [ style="filled" fillcolor="lightskyblue"]
    4 [ style="filled" fillcolor="green"]
    5 [ style="filled" fillcolor="lightskyblue"]
    6 [ style="filled" fillcolor="lightskyblue"]
    7 [ style="filled" fillcolor="lightskyblue"]
    8 [ style="filled" fillcolor="lightskyblue"]
    9 [ style="filled" fillcolor="lightskyblue"]
})";

    Flare::FlareGraph fg;
    std::istringstream istream(test1str);
    fg.loadGraphViz(istream);
    Flare::intset_t pdomgroup;
    bool ret = fg.getpdomGroup(pdomgroup, fg.getRootNode());
    REQUIRE(ret == true);
    fg.selectNodes(pdomgroup);
    std::ostringstream ostringstr;
    fg.storeGraphViz(ostringstr);
    //printf("Got output: '%s'\n", ostringstr.str().c_str());
    REQUIRE(ostringstr.str().c_str() == test1strsolution);

}
