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

#include "flusspferd/property_iterator.hpp"
#include "flusspferd/root.hpp"
#include "flusspferd/local_root_scope.hpp"
#include "flusspferd/exception.hpp"
#include "flusspferd/spidermonkey/init.hpp"
#include "flusspferd/spidermonkey/object.hpp"
#include <boost/optional.hpp>
#include <boost/utility/in_place_factory.hpp>
#include <js/jsapi.h>

// REf: spdmky 128
constexpr const jsid JSID_VOID;

using namespace flusspferd;

class property_iterator::impl {
public:
  impl()
    //: id(JSVAL_VOID) 
    //: id(JS::UndefinedValue().toInt32()) // Ref: https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/JS::Value#jsval
    // ref: spdmky 128
    : id(JSID_VOID)

  {}

  root_value root_iterator;
  object iterator;
  jsid id;
  root_value root_cache;
  // ref: spdmky 128
  //uint32 next;
  int32_t next;
};

property_iterator::property_iterator()
{}

property_iterator::property_iterator(object const &o_)
  : p(new impl)
{
  target = o_;

  local_root_scope scope;

  if (target.is_null())
    throw exception("Could not create property iterator (object is null)");

  // Ref: https://bugzilla.mozilla.org/attachment.cgi?id=8503746&action=diff
  /*JSObject *iterator =
    JS_NewPropertyIterator(Impl::current_context(), Impl::get_object(o));

  //if(!p->attrs.leng)
  //  throw exception("Could not create property iterator");

  /*p->iterator = Impl::wrap_object(iterator);
  p->root_iterator = p->iterator; */

  increment();
}

property_iterator::property_iterator(property_iterator const &o)
  : p(o.p ? new impl : 0)
{
  if (!p)
    return;

  if (!o.p->iterator.is_null()) {
    p->iterator = o.p->iterator;
    p->root_iterator = p->iterator;
    p->root_cache = value(o.p->root_cache);
  }
  p->id = o.p->id;
}

property_iterator::~property_iterator()
{}

void property_iterator::increment() {
  // Ref: https://bugzilla.mozilla.org/attachment.cgi?id=8503746&action=diff
  /*if (!JS_NextProperty(
        Impl::current_context(), Impl::get_object(p->iterator), &p->id))
    throw exception("Could not load / increment property iterator");*/

  JSObject* op = Impl::get_object(target);
  JS::IdVector attrs(Impl::current_context());
  JS_Enumerate(Impl::current_context(), JS::HandleObject::fromMarkedLocation(&op), JS::MutableHandle<JS::IdVector>::fromMarkedLocation(&attrs)); 
  // Get next and increase next position
  p->next++;
  if(p->next >= attrs.length())
    p->id = JSID_VOID;
  else
    p->id = attrs[p->next];


  // Ref: https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/JS_IdToValue
  jsval idVal;
  JS_IdToValue(Impl::current_context(), p->id, JS::MutableHandleValue::fromMarkedLocation(&idVal));
  if (idVal != JS::UndefinedValue()) {
    if (!JS_IdToValue(
          Impl::current_context(), p->id, JS::MutableHandleValue::fromMarkedLocation(Impl::get_jsvalp(p->root_cache))))
      throw exception("Could not load / increment property iterator");
  } else {
    p.reset();
  }
}

bool property_iterator::equal(property_iterator const &o) const {
  if (!p || !o.p)
    return !p && !o.p;
  return p->id == o.p->id;
}

value const &property_iterator::dereference() const {
  return p->root_cache;
}
