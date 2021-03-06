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

#include "flusspferd/arguments.hpp"
#include "flusspferd/exception.hpp"
#include "flusspferd/value.hpp"
#include <functional>
#include <cassert>
#include <js/jsapi.h>

using namespace flusspferd;

Impl::arguments_impl::arguments_impl(std::vector<value> const &vals)
  : n(vals.size()), argv(0)
{
	values.reserve(n);
	if (n > 0) {
		std::for_each(vals.begin(), vals.end(), [&](value v) {values.push_back(get_jsval(v)); });
		argv = &values[0];
	}
}

Impl::arguments_impl::arguments_impl(Impl::arguments_impl const &o)
  : values(o.values), n(o.n),
    argv(o.is_userprovided() ? 0 : o.argv)
{
  if (!argv)
    reset_argv();
}

Impl::arguments_impl &Impl::arguments_impl::operator=(arguments_impl const &o) {
  if(&o != this) {
    n = o.n;
    if(o.is_userprovided()) {
      values = o.values;
      reset_argv();
    }
    else {
      values.clear();
      argv = o.argv;
    }
  }
  return *this;
}

void Impl::arguments_impl::reset_argv() {
  //assert(is_userprovided());
  n = values.size();
  argv = n >  0 ? &values[0] : 0;
}

arguments::arguments(std::vector<value> const &v)
  : Impl::arguments_impl(v)
{ }

bool arguments::empty() const {
  return size() == 0;
}

std::size_t arguments::size() const {
  return Impl::arguments_impl::size();
}

value arguments::operator[](std::size_t i) {
  if (i >= size())
    return value();
  return Impl::wrap_jsvalp(get() + i);
}

void arguments::push_back(value const &v) {
  if(!is_userprovided())
    throw exception("trying to push data into system provided argument list");
  data().push_back(Impl::get_jsval(v));
  reset_argv();
}

void arguments::push_root(value const &v) {
  if(!is_userprovided())
    throw exception("trying to push data into system provided argument list");
  roots.push_back(boost::shared_ptr<root_value>(new root_value(v)));
  data().push_back(Impl::get_jsval(v));
  reset_argv();
}
    
value arguments::back() {
  if (empty())
    return value();
  return Impl::wrap_jsvalp(get() + size() - 1);
}

value arguments::front() {
  if (empty())
    return value();
  return Impl::wrap_jsvalp(get());
}

arguments::iterator &arguments::iterator::operator++() {
  Impl::arguments_impl::iterator_impl::operator++();
  return *this;
}

value arguments::iterator::operator*() const {
  jsval * v = Impl::arguments_impl::iterator_impl::operator*();
  assert(v);
  return Impl::wrap_jsvalp(v);
}

arguments::iterator arguments::begin() {
  return Impl::arguments_impl::iterator_impl(get());
}
  
arguments::iterator arguments::end() {
  return Impl::arguments_impl::iterator_impl(get() + size());
}

bool arguments::iterator::equals(arguments::iterator const &b) const {
  return *static_cast<Impl::arguments_impl::iterator_impl const&>(*this) ==
    *static_cast<Impl::arguments_impl::iterator_impl const&>(b);
}
