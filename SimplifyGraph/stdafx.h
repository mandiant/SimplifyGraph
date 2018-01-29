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


#pragma once

#ifndef __LINUX__
#  include "targetver.h"

#  define WIN32_LEAN_AND_MEAN             // Exclude rarely-used stuff from Windows headers
  // Windows Header Files:
#  include <windows.h>
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



//test this
#define USE_STANDARD_FILE_FUNCTIONS

// IDA SDK includes
// disable some warnings before the IDA includes
// mostly taken from the makeenv_vc.mak
#pragma warning( push )
#pragma warning( disable : 4244 ) // warning C4244: xxx : conversion from a to b, possible loss of data
#pragma warning( disable : 4267 ) // warning C4267: xxx : conversion from size_t to b, possible loss of data
//#pragma warning( disable : 4244 )

#include <pro.h>
#include <ida.hpp>
#include <idp.hpp>
#include <loader.hpp>
#include <kernwin.hpp>
#include <graph.hpp>
#include <name.hpp>
#include <funcs.hpp>
#include <bytes.hpp>
#include <auto.hpp>
#pragma warning ( pop )


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
