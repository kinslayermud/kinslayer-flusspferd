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

#include "flusspferd/native_object_base.hpp"
#include "flusspferd/tracer.hpp"
#include "flusspferd/object.hpp"
#include "flusspferd/string.hpp"
#include "flusspferd/call_context.hpp"
#include "flusspferd/root.hpp"
#include "flusspferd/exception.hpp"
#include "flusspferd/function.hpp"
#include "flusspferd/property_iterator.hpp"
#include "flusspferd/current_context_scope.hpp"
#include "flusspferd/spidermonkey/init.hpp"
#include <unordered_map>
#include <boost/variant.hpp>

using namespace flusspferd;

class native_object_base::impl {
public:
  //static void finalize(JSContext *ctx, JSObject *obj); // Ref: https://bug737365.bmoattachments.org/attachment.cgi?id=607922
  static void finalize(JSFreeOp *fop, JSObject *obj);
  static bool call_helper(JSContext *, JSObject *, uintN, jsval *, jsval *);

#if JS_VERSION >= 180
  static void trace_op(JSTracer *trc, JSObject *obj);
#else
  static uint32 mark_op(JSContext *, JSObject *, void *);
#endif

  template<property_mode>
  static bool property_op(JSContext *, JSObject *, jsval, jsval *);

  static bool new_resolve(JSContext *, JSObject *, jsval, uintN, JSObject **);

  //static bool new_enumerate(JSContext *cx, JSObject *obj,
  //  JSIterateOp enum_op, jsval *statep, jsid *idp);
  // Ref: https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/JSNewEnumerateOp
  // Ref: https://bug1097267.bmoattachments.org/attachment.cgi?id=8526676
  static bool new_enumerate(JSContext *cx, JSObject *obj, JS::AutoIdVector &properties);

public:
  static JSClass native_object_class;
  static JSClass native_enumerable_object_class;
};

static const unsigned int basic_flags =
  JSCLASS_HAS_PRIVATE
  //| JSCLASS_NEW_RESOLVE // Ref: https://bug993026.bmoattachments.org/attachment.cgi?id=8515452
#if JS_VERSION >= 180
  //| JSCLASS_MARK_IS_TRACE // Ref: https://bug638291.bmoattachments.org/attachment.cgi?id=516449
#endif
  ;

#if JS_VERSION >= 180
#define MARK_TRACE_OP ((JSMarkOp) &native_object_base::impl::trace_op)
#else
#define MARK_TRACE_OP (&native_object_base::impl::mark_op)
#endif

JSClass native_object_base::impl::native_object_class = {
  "NativeObject",
  basic_flags,
  &native_object_base::impl::property_op<native_object_base::property_add>,
  &native_object_base::impl::property_op<native_object_base::property_delete>,
  &native_object_base::impl::property_op<native_object_base::property_get>,
  &native_object_base::impl::property_op<native_object_base::property_set>,
  0, //JS_EnumerateStub, // Ref: https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/JSClass
  (JSResolveOp) &native_object_base::impl::new_resolve,
  0, //JS_ConvertStub, // Ref: https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/JSClass
  &native_object_base::impl::finalize,
  0,
  0,
  &native_object_base::impl::call_helper,
  0,
  0,
  0,
  MARK_TRACE_OP,
  0
};

JSClass native_object_base::impl::native_enumerable_object_class = {
  "NativeObject",
  basic_flags, // | JSCLASS_NEW_ENUMERATE, // Ref: https://bug1097267.bmoattachments.org/attachment.cgi?id=8526676
  &native_object_base::impl::property_op<native_object_base::property_add>,
  &native_object_base::impl::property_op<native_object_base::property_delete>,
  &native_object_base::impl::property_op<native_object_base::property_get>,
  &native_object_base::impl::property_op<native_object_base::property_set>,
  (JSEnumerateOp) &native_object_base::impl::new_enumerate,
  (JSResolveOp) &native_object_base::impl::new_resolve,
  0, //JS_ConvertStub, // https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/JSClass
  &native_object_base::impl::finalize,
  0,
  0,
  &native_object_base::impl::call_helper,
  0,
  0,
  0,
  MARK_TRACE_OP,
  0
};

native_object_base::native_object_base(object const &o) {
  p = new boost::scoped_ptr<impl>(new impl);
  load_into(o);
}

native_object_base::~native_object_base() {
  if(p)
  	delete p;
  if (!is_null()) {
    // Ref: https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/JS_SetPrivate
    //JS_SetPrivate(Impl::current_context(), get(), 0);
    JS_SetPrivate(get(), 0);
  }
}

void native_object_base::load_into(object const &o) {
  if (!is_null())
    throw exception("Cannot load native_object data into more than one object");

  object::operator=(o);

  if (!is_null()) {
    // Ref: https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/JS_SetPrivate
    //if (!JS_SetPrivate(Impl::current_context(), Impl::get_object(o), this))
    JS_SetPrivate(Impl::get_object(o), this);
    if (!JS_GetPrivate(Impl::get_object(o)))
      throw exception("Could not create native object (private data)");
  }
}

bool native_object_base::is_object_native(object const &o_) {
  object o = o_;

  if (o.is_null())
    return false;

  JSContext *ctx = Impl::current_context();
  JSObject *jso = Impl::get_object(o);
  // Ref: https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/JS_GET_CLASS
  //JSClass *classp = JS_GET_CLASS(ctx, jso);
  const JSClass *classp = JS_GetClass(jso);

  if (!classp || classp->finalize != &native_object_base::impl::finalize)
    return false;

  // Ref: https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/JS_GetPrivate
  //void *priv = JS_GetPrivate(ctx, jso);
  void *priv = JS_GetPrivate(jso);

  if (!priv)
    return false;

  return true;
}

native_object_base &native_object_base::get_native(object const &o_) {
  object o = o_;

  if (o.is_null())
    throw exception("Can not interpret 'null' as native object");

  JSContext *ctx = Impl::current_context();
  JSObject *jso = Impl::get_object(o);
  // Ref: https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/JS_GET_CLASS
  //JSClass *classp = JS_GET_CLASS(ctx, jso);
  const JSClass *classp = JS_GetClass(jso);

  if (!classp || classp->finalize != &native_object_base::impl::finalize)
    throw exception("Object is not native");

  // Ref: https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/JS_GetPrivate
  //void *priv = JS_GetPrivate(ctx, jso);
  void *priv = JS_GetPrivate(jso);

  if (!priv)
    throw exception("Object is not native");

  return *static_cast<native_object_base*>(priv);
}

object native_object_base::do_create_object(object const &prototype_) {
  JSContext *ctx = Impl::current_context();

  object prototype = prototype_;

  // Ref: https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/JS_NewObject
  /*JSObject *o = JS_NewObject(
      ctx,
      &impl::native_object_class,
      Impl::get_object(prototype),
      0); */
  
   JSObject *proto = Impl::get_object(prototype); 
   JSObject *o = JS_NewObjectWithGivenProto(ctx, &impl::native_object_class, JS::HandleObject::fromMarkedLocation(&proto)); 


  if (!o)
    throw exception("Could not create native object");

  return Impl::wrap_object(o);
}

object native_object_base::do_create_enumerable_object(object const &prototype_) {
  JSContext *ctx = Impl::current_context();

  object prototype = prototype_;

  // Ref: https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/JS_NewObject
  /* JSObject *o = JS_NewObject(
      ctx,
      &impl::native_enumerable_object_class,
      Impl::get_object(prototype),
      0)a */;

  JSObject *proto = Impl::get_object(prototype);
  JSObject *o = JS_NewObjectWithGivenProto(ctx, &impl::native_enumerable_object_class, JS::HandleObject::fromMarkedLocation(&proto));

  if (!o)
    throw exception("Could not create native object");

  return Impl::wrap_object(o);
}

//void native_object_base::impl::finalize(JSContext *ctx, JSObject *obj) { // REf: https://bug737365.bmoattachments.org/attachment.cgi?id=607922
void native_object_base::impl::finalize(JSFreeOp *fop, JSObject *obj) {
  // Ref: https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/JS_GetPrivate
  //void *p = JS_GetPrivate(ctx, obj);
  void *p = JS_GetPrivate(obj);

  if (p) {
    //current_context_scope scope(Impl::wrap_context(ctx)); // Ref: https://bug737365.bmoattachments.org/attachment.cgi?id=607922
    delete static_cast<native_object_base*>(p);
  }
}

bool native_object_base::impl::call_helper(
    JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
{
  FLUSSPFERD_CALLBACK_BEGIN {
    current_context_scope scope(Impl::wrap_context(ctx));

    //JSObject *function = JSVAL_TO_OBJECT(argv[-2]); // Ref: https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/JS_ValueToObject
    JSObject *function;
    JS_ValueToObject(ctx, JS::HandleValue::fromMarkedLocation(&argv[-2]), JS::MutableHandleObject::fromMarkedLocation(&function));

    native_object_base *self = 0;
    
    try {
      self = &native_object_base::get_native(Impl::wrap_object(obj));
    } catch (exception &) {
      self = &native_object_base::get_native(Impl::wrap_object(function));
    }

    call_context x;

    x.self = Impl::wrap_object(obj);
    x.self_native = self;
    x.arg = Impl::arguments_impl(argc, argv);
    x.result.bind(Impl::wrap_jsvalp(rval));
    x.function = Impl::wrap_object(function);

    self->self_call(x);
  } FLUSSPFERD_CALLBACK_END;
}

template<native_object_base::property_mode mode>
bool native_object_base::impl::property_op(
    JSContext *ctx, JSObject *obj, jsval id, jsval *vp)
{
  FLUSSPFERD_CALLBACK_BEGIN {
    current_context_scope scope(Impl::wrap_context(ctx));

    native_object_base &self =
      native_object_base::get_native(Impl::wrap_object(obj));

    value data(Impl::wrap_jsvalp(vp));
    self.property_op(mode, Impl::wrap_jsval(id), data);
  } FLUSSPFERD_CALLBACK_END;
}

bool native_object_base::impl::new_resolve(
    JSContext *ctx, JSObject *obj, jsval id, uintN sm_flags, JSObject **objp)
{
  FLUSSPFERD_CALLBACK_BEGIN {
    current_context_scope scope(Impl::wrap_context(ctx));

    native_object_base &self =
      native_object_base::get_native(Impl::wrap_object(obj));

    unsigned flags = 0;

    // Ref: https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/JSNewResolveOp
    //if (sm_flags & JSRESOLVE_QUALIFIED)
    //  flags |= property_qualified;
    //if (sm_flags & JSRESOLVE_ASSIGNING) // Ref: https://bug547140.bmoattachments.org/attachment.cgi?id=8397245
    //  flags |= property_assigning;
    //if (sm_flags & JSRESOLVE_DETECTING)
    //  flags |= property_detecting;
    //if (sm_flags & JSRESOLVE_DECLARING)
    //  flags |= property_declaring;
    //if (sm_flags & JSRESOLVE_CLASSNAME)
    //  flags |= property_classname;

    *objp = 0;
    if (self.property_resolve(Impl::wrap_jsval(id), flags))
      *objp = Impl::get_object(self);
  } FLUSSPFERD_CALLBACK_END;
}

//bool native_object_base::impl::new_enumerate(
//    JSContext *ctx, JSObject *obj, JSIterateOp enum_op, jsval *statep, jsid *idp)
bool native_object_base::impl::new_enumerate(JSContext *ctx, JSObject *obj, JS::AutoIdVector &properties)
{
  // Ref:  https://bug1097267.bmoattachments.org/attachment.cgi?id=8526676   // return the properties of the object in output properties
  /*
  FLUSSPFERD_CALLBACK_BEGIN {
    current_context_scope scope(Impl::wrap_context(ctx));

    native_object_base &self =
      native_object_base::get_native(Impl::wrap_object(obj));

    boost::any *iter;
    switch (enum_op) {
    case JSENUMERATE_INIT:
      {
        iter = new boost::any;
        int num = 0;
        *iter = self.enumerate_start(num);
	// Ref: https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/PRIVATE_TO_JSVAL
        //*statep = PRIVATE_TO_JSVAL(iter);
	*statep = JS::PrivateValue(iter);
        if (idp) {
	  // Ref: https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/jsid
	  // Ref: https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/JS::Int32Value
          //*idp = INT_TO_JSVAL(num);
	  jsval val = JS::Int32Value(num);
	  JS_ValueToId(ctx, JS::HandleValue::fromMarkedLocation(&val), JS::MutableHandleId::fromMarkedLocation(idp));
	}
        return true;
      }
    case JSENUMERATE_NEXT:
      {
	// Ref: https://bug952650.bmoattachments.org/attachment.cgi?id=8413511
        //iter = (boost::any*)JSVAL_TO_PRIVATE(*statep);
	iter = (boost::any*)statep->toPrivate();
        value id;
        if (iter->empty() || (id = self.enumerate_next(*iter)).is_undefined())
          // Ref: https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/JS::UndefinedValue
          //*statep = JSVAL_NULL;
	  *statep = JS::UndefinedValue();
        else {
          JS_ValueToId(ctx, Impl::get_jsval(id), idp);
        }
        return true;
      }
    case JSENUMERATE_DESTROY:
      {
	// Ref: https://bug952650.bmoattachments.org/attachment.cgi?id=8413511
        //iter = (boost::any*)JSVAL_TO_PRIVATE(*statep);
	iter = (boost::any*)statep->toPrivate();
        delete iter;
        return true;
      }
    }
  } FLUSSPFERD_CALLBACK_END;

  */
  object o = Impl::wrap_object(obj);
  for (property_iterator it = o.begin(); it != o.end(); ++it) {
    jsid property;
    value v = *it;
    jsval val = JS::Int32Value(v.get_int());
    JS_ValueToId(ctx, JS::HandleValue::fromMarkedLocation(&val), JS::MutableHandleId::fromMarkedLocation(&property));
    properties.append(property); 
  }

  return true;
}

#if JS_VERSION >= 180
void native_object_base::impl::trace_op(
    JSTracer *trc, JSObject *obj)
{
  //current_context_scope scope(Impl::wrap_context(trc->context)); // Ref: JSTracer is deprecated: https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/Internals/Tracing_JIT
  current_context_scope scope(Impl::wrap_context(Impl::current_context()));

  native_object_base &self =
    native_object_base::get_native(Impl::wrap_object(obj));

  tracer tracer_(trc);
  self.trace(tracer_);
}
#else
uint32 native_object_base::impl::mark_op(
    JSContext *ctx, JSObject *obj, void *thing)
{
  current_context_scope scope(Impl::wrap_context(ctx));

  native_object_base &self =
    native_object_base::get_native(Impl::wrap_object(obj));

  tracer trc(thing);
  self.trace(trc);

  return 0;
}
#endif

void native_object_base::property_op(
    property_mode, value const &, value &)
{
}

bool native_object_base::property_resolve(value const &, unsigned) {
  return false;
}

boost::any native_object_base::enumerate_start(int &)
{
  return boost::any();
}

value native_object_base::enumerate_next(boost::any &)
{
  return value();
}

void native_object_base::self_call(call_context &) {
  throw exception("Object can not be called");
}

void native_object_base::trace(tracer&) {}
