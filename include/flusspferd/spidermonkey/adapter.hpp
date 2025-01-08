// vim:ts=2:sw=2:expandtab:autoindent:filetype=cpp:
/*
The MIT License

Copyright (c) 2008, 2009 Flusspferd contributors (see "CONTRIBUTORS" or
                                       http://flusspferd.org/contributors.txt)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/

#ifndef FLUSSPFERD_SPIDERMONKEY_ADAPTER_HPP
#define FLUSSPFERD_SPIDERMONKEY_ADAPTER_HPP

//#include <js/js/LegacyIntTypes.h>
#include <js/jsapi.h>

#define JS_VERSION 200  // Something higher than 180 does the work

#define INT_TO_JSVAL JS::Int32Value
#define INT_FITS_IN_JSVAL  true

typedef JS::Value jsval;
typedef unsigned jsuint;
typedef unsigned uintN;
typedef double jsdouble;

// Redefine our own functions to avoid undefined references for those functions no longer present in mozilla's code
#define moz_arena_malloc(arena, bytes) js_arena_calloc(js::MallocArena, bytes)
#define moz_arena_calloc(arena, bytes) js_arena_calloc(js::MallocArena, bytes)
#define moz_arena_realloc(arena, p, bytes) js_arena_realloc(js::MallocArena, p, bytes)

#endif // FLUSSPFERD_SPIDERMONKEY_ADAPTER_HPP
