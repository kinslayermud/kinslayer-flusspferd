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

#include "flusspferd/value.hpp"
#include "flusspferd/object.hpp"
#include "flusspferd/string.hpp"
#include "flusspferd/exception.hpp"
#include "flusspferd/spidermonkey/init.hpp"
#include "flusspferd/spidermonkey/object.hpp"
#include "flusspferd/spidermonkey/string.hpp"
#include <js/jsapi.h>
#include <cassert>
#include <cmath>

#ifdef _MSC_VER
#include <float.h>
#endif

using namespace flusspferd;

value::~value() { }
value::value() : Impl::value_impl(JS::UndefinedValue()) { }

// Ref: https://bugzilla.mozilla.org/show_bug.cgi?id=952650
bool value::is_null() const { return get().isNull(); }
bool value::is_undefined() const { return get().isUndefined(); }
bool value::is_int() const { return get().isInt32(); }
bool value::is_double() const { return get().isDouble(); }
bool value::is_number() const { return get().isNumber(); }
bool value::is_boolean() const { return get().isBoolean(); }
bool value::is_string() const { return get().isString(); }
bool value::is_object() const { return get().isObject(); }
bool value::is_function() const {
  jsval val = get();
  return JS_TypeOfValue(Impl::current_context(), JS::HandleValue::fromMarkedLocation(&val)) == JSTYPE_FUNCTION;
}

bool value::get_boolean() const {
  assert(is_boolean());
  return get().toBoolean();
}
int value::get_int() const {
  assert(is_int());
  return get().toInt32();
}
double value::get_double() const {
  assert(is_double());
  jsdouble d = get().toDouble();
  return d;
}
object value::get_object() const {
  assert(is_object());
  return Impl::wrap_object(get().toObjectOrNull());
}
string value::get_string() const {
  assert(is_string());
  return Impl::wrap_string(get().toString());
}

string value::to_string() const {
  return string(*this);
}

std::string value::to_std_string() const {
  return string(*this).to_string();
}

double value::to_number() const {
  double value;
  // Ref: https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/JS::Value
  //if (!JS_ValueToNumber(Impl::current_context(), get(), &value))
  if (!get().isNumber())
    throw exception("Could not convert value to number");
  value = get().toNumber();
  return value;
}

double value::to_integral_number(int bits, bool signedness) const {
  long double value = to_number();
#ifdef _MSC_VER
  if (!_finite(value))
    return 0;
#else
  if (!std::isfinite(value))
    return 0;
#endif
  long double maxU = powl(2, bits);
  value = value < 0 ? ceill(value) : floorl(value);
  value = fmodl(value, maxU);
  if (value < 0)
    value += maxU;
  if (signedness) {
    long double maxS = maxU / 2;
    if (value > maxS)
      value -= maxU;
  }
  return value;
}

bool value::to_boolean() const {
  bool result;
  // Ref: https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/JS::Value
  //if (!JS_ValueToBoolean(Impl::current_context(), get(), &result))
  if (!get().isBoolean())
    throw exception("Could not convert value to boolean");
  result = get().toBoolean();
  return result != false;
}

object value::to_object() const {
  JSObject *result;
  if (!JS_ValueToObject(Impl::current_context(), get(), &result))
    throw exception("Could not convert value to object");
  return Impl::wrap_object(result);
}

string value::to_source() const {
#ifdef JS_ValueToSource
  JSString *source = JS_ValueToSource(Impl::current_context(), get());

  if (!source)
    throw exception("Could not convert value to source");
  return Impl::wrap_string(source);
#else
  // This is potentially dangerous. Not sure there's much other choice if we
  // want to support older spidermonkey versions though
  return current_context().global().call("uneval", *this);
#endif
}

void value::bind(value o) {
  setval(JS::UndefinedValue());
  setp(o.getp());
}

void value::unbind() {
  setval(JS::UndefinedValue());
  setp(getvalp());
}

Impl::value_impl Impl::value_impl::from_double(double num) {
  value_impl result = JS_NumberValue(num);
  // Ref: https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/JS_NumberValue
  //if (!JS_NewNumberValue(
  //      Impl::current_context(), jsdouble(num), result.getp()))
  //{
  //  throw exception("Could not convert integer to value");
  //}
  return result;
}

// Ref: https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/JS::Value#jsval

Impl::value_impl Impl::value_impl::from_boolean(bool x) {
  return wrap_jsval(JS::BooleanValue(x));
}

Impl::value_impl Impl::value_impl::from_string(string const &s) {
  return wrap_jsval(JS::StringValue(Impl::get_string(const_cast<string&>(s))));
}

Impl::value_impl Impl::value_impl::from_object(object const &o) {
  return wrap_jsval(JS::ObjectValue(Impl::get_object(const_cast<object&>(o))));
}
