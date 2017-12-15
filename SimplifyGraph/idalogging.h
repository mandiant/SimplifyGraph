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

// Contains debug wrappers for logging & execution that work both in IDA and 
// outside for testing

#ifndef FLARE_LOGGING
#define FLARE_LOGGING


#ifdef _DEBUG
#define DEBUGEXEC(x) do { x } while(0)

#ifdef idaapi 
#define DEBUGLOG(format, ...) msg(format, __VA_ARGS__)
#else
#define DEBUGLOG(format, ...) fprintf(stderr, format, __VA_ARGS__)
#endif // #ifdef _DEBUG

#else  // #ifdef _DEBUG
#define DEBUGLOG(format, ...)
#define DEBUGEXEC(x) do { } while(0)
#endif // #ifdef _DEBUG

#ifdef idaapi 
#define LOG(format, ...) msg(format, __VA_ARGS__)
#else
#define LOG(format, ...) fprintf(stderr, format, __VA_ARGS__)
#endif // #ifdef _DEBUG


#endif //#ifndef FLARE_LOGGING
