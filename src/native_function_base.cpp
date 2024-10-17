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
#include "flusspferd/native_function_base.hpp"
#include "flusspferd/call_context.hpp"
#include "flusspferd/local_root_scope.hpp"
#include "flusspferd/exception.hpp"
#include "flusspferd/arguments.hpp"
#include "flusspferd/tracer.hpp"
#include "flusspferd/spidermonkey/init.hpp"
#include "flusspferd/spidermonkey/context.hpp"
#include "flusspferd/spidermonkey/function.hpp"
#include "flusspferd/current_context_scope.hpp"
#include <js/jsapi.h>
#include <js/Object.h>

using namespace flusspferd;

class native_function_base::impl {
public:
  impl(unsigned arity, std::string const &name)
  : arity(arity), name(name)
  {}

  // Ref: https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/JSNative
  //static bool call_helper(
  //  JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval);
  static bool call_helper(JSContext *ctx, uintN argc, jsval *argv);
  // Ref: https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/JSFinalizeOp
  //static void finalize(JSContext *, JSObject *);
  //static void finalize(JSFreeOp *fop, JSObject *);
  static void finalize(JS::GCContext* gcx, JSObject *);

#if JS_VERSION >= 180
  static void trace_op(JSTracer *trc, JSObject *obj);
#else
  static uint32 mark_op(JSContext *, JSObject *, void *);
#endif

  unsigned arity;
  std::string name;

  static JSClass function_priv_class;
};

native_function_base::native_function_base(unsigned arity)
: p(new impl(arity, std::string()))
{}

native_function_base::native_function_base(
  unsigned arity,
  std::string const &name
)
  : p(new impl(arity, name))
{}

native_function_base::~native_function_base() { }


#if JS_VERSION >= 180
// Ref: https://bug638291.bmoattachments.org/attachment.cgi?id=516449
//#define MARK_TRACE_OP ((JSMarkOp) &native_function_base::impl::trace_op)
#define MARK_TRACE_OP ((JSTraceOp) &native_function_base::impl::trace_op)
#else
#define MARK_TRACE_OP (&native_function_base::impl::mark_op)
#endif

JSClassOps native_function_base::ops1 = {
  0,
  0,
  0,
  0,
  0,
  0,
  &native_function_base::impl::finalize,
  0,
  0,
  MARK_TRACE_OP
};

JSClass native_function_base::impl::function_priv_class = {
  "FunctionParent", 0, &ops1
};

#undef MARK_TRACE_OP

function native_function_base::create_function() {
  JSContext *ctx = Impl::current_context();

  JSFunction *fun;

  {
    local_root_scope scope;

    // Ref: https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/JS_NewObject
    //JSObject *priv = JS_NewObject(ctx, &impl::function_priv_class, 0, 0);
    JSObject *priv = JS_NewObject(ctx, &impl::function_priv_class);

    if (!priv)
      throw exception("Could not create native function");

    //JS_SetPrivate(ctx, priv, this); // Ref: https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/JS_SetPrivate
    //JS_SetPrivate(priv, this);

    // https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/JS_NewFunction (parent removed)
    // fun = JS_NewFunction(ctx, &impl::call_helper, p->arity, 0, 0, p->name.c_str());
    fun = JS_NewFunction(
        ctx, &impl::call_helper,
        p->arity, 0, p->name.c_str());

    if (!fun)
      throw exception("Could not create native function");

    function::operator=(Impl::wrap_function(fun));

    JSObject *obj = Impl::get_object(*this);

    //JS_SetReservedSlot(ctx, obj, 0, OBJECT_TO_JSVAL(priv)); // Ref: https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/JS::ObjectValue && https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/JS_GetReservedSlot
    // Ref: spdmky 128
    // JS_SetReservedSlot(obj, 0, JS::ObjectValue(*priv));
    JS::SetReservedSlot(obj, 0, JS::ObjectValue(*priv));
    //JS_SetReservedSlot(ctx, obj, 1, PRIVATE_TO_JSVAL(this)); // Ref: https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/PRIVATE_TO_JSVAL && https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/JS_GetReservedSlot
    // Ref: spdmky 128
    //  JS_SetReservedSlot(obj, 1, JS::PrivateValue(this));
    JS::SetReservedSlot(obj, 1, JS::PrivateValue(this));
  }

  return *static_cast<function *>(this);
}

// Ref: https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/JSNative
// typedef bool (* JSNative)(JSContext *cx, unsigned argc, JS::Value *vp);
//bool native_function_base::impl::call_helper(
//    JSContext *ctx, JSObject *obj, uintN argc, jsval *argv, jsval *rval)
bool native_function_base::impl::call_helper(JSContext *ctx,  uintN argc, jsval *argv)
{
  FLUSSPFERD_CALLBACK_BEGIN {
    current_context_scope scope(Impl::wrap_context(ctx));

    JSObject *function;
    JS_ValueToObject(ctx, JS::HandleValue::fromMarkedLocation(&argv[-2]), JS::MutableHandleObject::fromMarkedLocation(&function));

    //JSObject *function = JSVAL_TO_OBJECT(argv[-2]); // Ref: https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/JS_ValueToObject

    jsval self_val;

    //if (!JS_GetReservedSlot(ctx, function, 1, &self_val)) // https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/JS_GetReservedSlot
    // ref: spdmky 128
    //self_val = JS_GetReservedSlot(function, 1);
    //if (self_val.isNull()) // https://bug952650.bmoattachments.org/attachment.cgi?id=8413485
    //  throw exception("Could not call native function");


    // native_function_base *self = (native_function_base *)JSVAL_TO_PRIVATE(self_val); // https://bug952650.bmoattachments.org/attachment.cgi?id=8413511
    native_function_base *self = (native_function_base *) self_val.toPrivate();

    if (!self)
      throw exception("Could not call native function");

    call_context x;

    //x.self = Impl::wrap_object(obj);
    x.arg = Impl::arguments_impl(argc, argv);
    //x.result.bind(Impl::wrap_jsvalp(rval));
    x.function = Impl::wrap_object(function);

    self->call(x);
  } FLUSSPFERD_CALLBACK_END;
}


#if JS_VERSION >= 180
void native_function_base::impl::trace_op(
    JSTracer *trc, JSObject *obj)
{
  //current_context_scope scope(Impl::wrap_context(trc->context));
  current_context_scope scope(Impl::wrap_context(Impl::current_context()));

  native_function_base *self =
    native_function_base::get_native(Impl::wrap_object(obj));

  tracer tracer_(trc);
  self->trace(tracer_);
}
#else
uint32 native_function_base::impl::mark_op(
    JSContext *ctx, JSObject *obj, void *thing)
{
  current_context_scope scope(Impl::wrap_context(ctx));

  native_function_base *self =
    native_function_base::get_native(Impl::wrap_object(obj));

  tracer trc(thing);
  self->trace(trc);

  return 0;
}
#endif

// Ref: https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/JSFinalizeOp
//void native_function_base::impl::finalize(JSContext *ctx, JSObject *priv) {
void native_function_base::impl::finalize(JS::GCContext* gcx, JSObject *priv) {
  JSContext *ctx = Impl::current_context();
  current_context_scope scope(Impl::wrap_context(ctx));

#ifdef ENABLED
  native_function_base *self =
    //(native_function_base *) JS_GetInstancePrivate(ctx, priv, &function_priv_class, 0); // Ref: https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/JS_GetInstancePrivate
    (native_function_base *) JS_GetInstancePrivate(ctx, JS::HandleObject::fromMarkedLocation(&priv), &function_priv_class, 0);

  if (!self)
    throw exception("Could not finalize native function");

  delete self;
#endif
}

native_function_base *native_function_base::get_native(object const &o_) {
  JSContext *ctx = Impl::current_context();

  object o = o_;
  JSObject *p = Impl::get_object(o);

#ifdef ENABLED
  native_function_base *self =
    //(native_function_base *) JS_GetInstancePrivate(ctx, p, &impl::function_priv_class, 0); // Ref: https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/JS_GetInstancePrivate 
    (native_function_base *) JS_GetInstancePrivate(ctx, JS::HandleObject::fromMarkedLocation(&p), &impl::function_priv_class, 0);

  if (self)
    return self;

  //if (!JS_ObjectIsFunction(ctx, p))
  if (!JS_ObjectIsFunction(p))
    throw exception("Could not get native function pointer (no function)");

  jsval p_val;

  //if (!JS_GetReservedSlot(ctx, p, 0, &p_val)) // Ref:: https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/JS_GetReservedSlot
  p_val = JS_GetReservedSlot(p, 0);
  if (p_val.isNull()) // Ref: https://bug952650.bmoattachments.org/attachment.cgi?id=8413485
    throw exception("Could not get native function pointer");

  JS_ValueToObject(ctx, JS::HandleValue::fromMarkedLocation(&p_val), JS::MutableHandleObject::fromMarkedLocation(&p));

  //p = JSVAL_TO_OBJECT(p_val); // Ref: https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/JS_ValueToObject

  if (!p)
    throw exception("Could not get native function pointer");

  self =
    //(native_function_base *) JS_GetInstancePrivate(ctx, p, &impl::function_priv_class, 0); // Ref: https://udn.realityripple.com/docs/Mozilla/Projects/SpiderMonkey/JSAPI_reference/JS_GetInstancePrivate
    (native_function_base *) JS_GetInstancePrivate(ctx, JS::HandleObject::fromMarkedLocation(&p), &impl::function_priv_class, 0);

  if (!self)
    throw exception("Could not get native function pointer");

  return self;
#endif
  return nullptr;
}

void native_function_base::trace(tracer&) {}
