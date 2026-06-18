// runtime/src/function_handles.cpp
//
// Function-handle runtime builtins (str2func / func2str), extracted from
// toolboxes/builtin's Pack 13 into the runtime layer (L2, engine-coupled
// scripting runtime — NOT a math toolbox). They operate the engine: str2func
// parses an anonymous-function source via engine.eval and mints named handles
// through the engine allocator; func2str reads a handle's stored name. Neither
// has a core-free form, hence runtime (cf. feval, which stays in builtin for
// now because its pausable FevalCallbackBuiltin adapter belongs with the
// callback adapters that move to bundle in step F). Behaviour is unchanged;
// only the owning translation unit / layer differs. registerFunctionHandles is
// composed by installRuntimeLibrary (runtime.cpp), which
// bundle/installStandardLibrary calls.
#include <numkit/runtime/runtime.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/value/span.hpp>
#include <numkit/value/value.hpp>

#include <stdexcept>
#include <string>

namespace numkit::runtime {

void registerFunctionHandles(Engine &engine)
{
    // str2func('name') — create a function handle by name.
    engine.registerFunction("str2func",
                            [](Span<const Value> args, size_t /*nargout*/,
                               Span<Value> outs, CallContext &ctx) {
                                if (args.empty())
                                    throw std::runtime_error("str2func requires 1 argument");
                                if (!args[0].isChar() && !args[0].isString())
                                    throw std::runtime_error(
                                        "str2func: argument must be a string");
                                std::string s = args[0].toString();
                                // MATLAB str2func accepts three forms:
                                //   'sin'        -> named handle
                                //   '@sin'       -> named handle (strip '@')
                                //   '@(x) x+1'   -> anonymous function (parse)
                                // Previously the whole string (including any
                                // leading '@') was stored as the handle NAME,
                                // so '@sin' / '@(x)..' became unfindable '@@..'.
                                if (!s.empty() && s[0] == '@') {
                                    size_t j = 1;
                                    while (j < s.size() && (s[j] == ' ' || s[j] == '\t'))
                                        ++j;
                                    if (j < s.size() && s[j] == '(') {
                                        // Anonymous function source — parse it.
                                        outs[0] = ctx.engine->eval(s, true);
                                    } else {
                                        outs[0] = Value::funcHandle(
                                            s.substr(j), ctx.engine->resource());
                                    }
                                } else {
                                    outs[0] = Value::funcHandle(
                                        s, ctx.engine->resource());
                                }
                            });

    // func2str(@fn) — recover a function handle's text.
    // MATLAB: named handles (`@sin`) return the bare name `'sin'`; anonymous
    // handles return their reconstructed source (`@(x) x*2` -> `'@(x)x*2'`). The
    // parser stores that source on the handle (reconstructed from the token
    // stream, MATLAB-normalized whitespace); we return it here. If it's somehow
    // absent (e.g. a VM closure-with-captures, represented as a cell) we fall
    // back to the internal `@__anon_<N>` placeholder.
    engine.registerFunction("func2str",
                            [](Span<const Value> args, size_t /*nargout*/,
                               Span<Value> outs, CallContext &ctx) {
                                if (args.empty())
                                    throw std::runtime_error("func2str requires 1 argument");
                                // A VM closure that captures variables is packed
                                // as a cell {handle, captures...} (the same
                                // convention Engine::callFunctionHandleMulti uses
                                // to make it callable). Unwrap to the bare handle
                                // so func2str works on captured anon functions too
                                // (e.g. @(x) x + a). On TreeWalker / for capture-
                                // free anons the handle is plain — this is a no-op.
                                const Value *h = &args[0];
                                if (h->isCell() && h->numel() >= 1 &&
                                    h->cellAt(0).isFuncHandle())
                                    h = &h->cellAt(0);
                                if (!h->isFuncHandle())
                                    throw std::runtime_error(
                                        "func2str: argument must be a function handle");
                                const std::string name = h->funcHandleName();
                                // Anonymous handles carry their source text;
                                // named handles (sin, foo, …) return the name.
                                const bool isAnon = name.rfind("__anon_", 0) == 0;
                                std::string text;
                                if (isAnon) {
                                    const std::string src = h->funcHandleSource();
                                    text = !src.empty() ? src : ("@" + name);
                                } else {
                                    text = name;
                                }
                                outs[0] = Value::fromString(text, ctx.engine->resource());
                            });
}

} // namespace numkit::runtime
