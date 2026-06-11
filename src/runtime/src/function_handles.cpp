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

    // func2str(@fn) — recover the name of a function handle.
    // MATLAB: named handles (`@sin`) return just the bare name `'sin'`;
    // anonymous handles (`@(x) x*2`) return the full `'@(x)x*2'` source
    // text. We don't store anon source text, so fall back to the
    // internal `__anon_<N>` name with `@` prefix for those — best-
    // effort placeholder. See BUGS.md #16.
    engine.registerFunction("func2str",
                            [](Span<const Value> args, size_t /*nargout*/,
                               Span<Value> outs, CallContext &ctx) {
                                if (args.empty())
                                    throw std::runtime_error("func2str requires 1 argument");
                                if (!args[0].isFuncHandle())
                                    throw std::runtime_error(
                                        "func2str: argument must be a function handle");
                                const std::string name = args[0].funcHandleName();
                                // Detect anon-handle naming convention: parser
                                // assigns `__anon_<N>` to lambdas. Named
                                // handles (sin, foo, etc.) get the bare name.
                                const bool isAnon = name.rfind("__anon_", 0) == 0;
                                outs[0] = Value::fromString(
                                    isAnon ? ("@" + name) : name,
                                    ctx.engine->resource());
                            });
}

} // namespace numkit::runtime
