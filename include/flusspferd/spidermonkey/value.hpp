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

#ifndef FLUSSPFERD_SPIDERMONKEY_VALUE_HPP
#define FLUSSPFERD_SPIDERMONKEY_VALUE_HPP

#include <jsapi.h>

namespace flusspferd {

class string;
class object;

#ifndef IN_DOXYGEN

namespace Impl {

class value_impl {
  JS::Value val;
  JS::Value *ref;

protected:
  JS::Value get() const { return *ref; }
  JS::Value *getp()     { return ref; }
  void set(JS::Value v) { *ref = v; }
  void setp(JS::Value *p) { ref = p; }
  void setval(JS::Value v) { val = v; }
  JS::Value *getvalp() { return &val; }

  value_impl(JS::Value v) : val(v), ref(&val) { }
  value_impl(JS::Value *v) : val(JS::UndefinedValue()), ref(v) { }
  value_impl() : val(JS::UndefinedValue()), ref(&val) { }

  friend JS::Value get_jsval(value_impl const &v);
  friend value_impl wrap_jsval(JS::Value v);
  friend JS::Value *get_jsvalp(value_impl &v);
  friend value_impl wrap_jsvalp(JS::Value *p);

  template<typename T>
  static value_impl from_integer(T const &num);
  static value_impl from_double(double num);
  static value_impl from_boolean(bool x);
  static value_impl from_string(string const &x);
  static value_impl from_object(object const &x);

public:
  value_impl(value_impl const &o) {
    if (o.ref == &o.val) {
      val = o.val;
      ref = &val;
    } else {
      val = JS::UndefinedValue();
      ref = o.ref;
    }
  }

  value_impl &operator=(value_impl const &o) {
    *ref = *o.ref;
    return *this;
  }

  void *get_gcptr() {
    return getp();
  }
};

JS::RootedValue get_rooted_handle(value_impl const &v);
JS::MutableHandleValue get_mutable_handle(value_impl const &v);

inline JS::Value get_jsval(value_impl const &v) {
  return v.get();
}

inline value_impl wrap_jsval(JS::Value v) {
  return value_impl(v);
}

inline JS::Value *get_jsvalp(value_impl &v) {
  return v.getp();
}

inline value_impl wrap_jsvalp(JS::Value *p) {
  return value_impl(p);
}

template<typename T>
value_impl value_impl::from_integer(T const &num) {
  return wrap_jsval(JS::Int32Value(num));
}

}

#endif

}

#endif /* FLUSSPFERD_SPIDERMONKEY_VALUE_HPP */
