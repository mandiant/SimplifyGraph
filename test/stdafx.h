// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#ifdef _MSC_VER
#  include "targetver.h"

#  include <windows.h>
#  include <tchar.h>
#else
#  define  TCHAR char
#  define _TCHAR char
#  define _TSCHAR signed char
#  define _TUCHAR unsigned char
#  define _TXCHAR unsigned char
#  define _T(s) s
#endif

// c++ includes
#include <sstream>
#include <algorithm>
#include <exception>
#include <map>
#include <set>
#include <deque>
#include <vector>
#include <string>
#include <utility>


#include <stdio.h>
#include <stdint.h>

// Boost includes
#include <boost/algorithm/string.hpp>
#include <boost/graph/reverse_graph.hpp>
#include <boost/graph/adjacency_list.hpp>
#include <boost/bind.hpp>
#include <boost/foreach.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/graph/graphviz.hpp>
#include <boost/graph/dominator_tree.hpp>
#include <boost/graph/reverse_graph.hpp>
#include <boost/graph/transpose_graph.hpp>


// TODO: reference additional headers your program requires here
