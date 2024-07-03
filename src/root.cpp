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


#include "flusspferd/spidermonkey/adapter.hpp"

#include "flusspferd/root.hpp"
#include "flusspferd/init.hpp"
#include "flusspferd/exception.hpp"
#include "flusspferd/value.hpp"
#include "flusspferd/object.hpp"
#include "flusspferd/string.hpp"
#include "flusspferd/function.hpp"
#include "flusspferd/array.hpp"
#include "flusspferd/spidermonkey/init.hpp"
#include "flusspferd/spidermonkey/context.hpp"
#include "flusspferd/spidermonkey/value.hpp"
#include <js/jsapi.h>

namespace flusspferd { namespace detail {

template<typename T>
root<T>::root(T const &o)
: T(o)
{
  // Ref: https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/JS::PersistentRooted
  // Ref: 
  if (std::is_same<T, value>::value) {      
    value* v = (value*)T::get_gcptr();
    jsval* val = Impl::get_jsvalp(*v);
    *pr = *val;
  } else if (std::is_same<T, object>::value) {
    object* o = (object*)T::get_gcptr();
    JSObject* obj = Impl::get_object(*o);
    *po = obj;
  } else if (std::is_same<T, string>::value) {
    string* s = (string*)T::get_gcptr();
    value v(s); 
    jsval* val = Impl::get_jsvalp(v);
    *pr = *val;
  } else if (std::is_same<T, function>::value) {
    function* f = (function*)T::get_gcptr();
    JSObject* obj = Impl::get_object(*f);
    *po = obj;
  } else if (std::is_same<T, array>::value) {
    array* a = (array*)T::get_gcptr();
    JSObject* obj = Impl::get_object(*a);
    *po = obj;
  }  

  /*
    bool status;

    status = JS_AddRoot(
    Impl::current_context(),
    T::get_gcptr());

  if (status == false) {
    throw exception("Cannot root Javascript value");
  } */
}

template<typename T>
root<T>::~root() {
  // Ref: https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/JS::PersistentRooted
  // No destroy/deallocation required
  /* JS_RemoveRoot(
    Impl::current_context(),
    T::get_gcptr()); */
}

template class root<value>;
template class root<object>;
template class root<string>;
template class root<function>;
template class root<array>;

}}
