// nk_codegen_rt.cpp — implementation of the Value-ABI bridge (see the
// header + DESIGN.md §6a). numkit (Value + a private StandardEngine) lives
// entirely behind the opaque C ABI; nothing leaks to the generated code.

#include "nk_codegen_rt.h"

#include <numkit/bundle/standard_engine.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/value/span.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/value_type.hpp>

#include <string>
#include <unordered_map>
#include <vector>

namespace {

using numkit::Value;

// The encapsulated default engine — it carries the builtin registry. A
// function-local static (built on first use), NOT a global the generated
// code can see.
numkit::StandardEngine &engine()
{
    static numkit::StandardEngine e;
    return e;
}

// Function handle for `name` (`@name`), cached. Resolves builtins and
// path functions exactly as the interpreter would.
const Value &handleFor(const std::string &name)
{
    static std::unordered_map<std::string, Value> cache;
    const auto it = cache.find(name);
    if (it != cache.end()) return it->second;
    // suppressTopLevelDisplay = true: don't print `ans = @name`.
    return cache.emplace(name, engine().eval("@" + name, true)).first->second;
}

Value *unwrap(nk_val v) { return reinterpret_cast<Value *>(v); }
nk_val  wrap(Value *v) { return reinterpret_cast<nk_val>(v); }

} // namespace

extern "C" {

nk_val nk_box_scalar(double v) { return wrap(new Value(Value::scalar(v))); }

nk_val nk_box_array(const double *p, size_t len)
{
    Value   m = Value::matrix(1, len, numkit::ValueType::DOUBLE, nullptr);
    double *d = m.doubleDataMut();
    for (size_t i = 0; i < len; ++i) d[i] = p[i];
    return wrap(new Value(std::move(m)));
}

nk_val nk_call(const char *name, const nk_val *args, size_t nargs,
               size_t nargout, nk_val *extra_outs)
{
    std::vector<Value> a;
    a.reserve(nargs);
    for (size_t i = 0; i < nargs; ++i) a.push_back(*unwrap(args[i]));

    const size_t       nout = nargout == 0 ? 1 : nargout;
    std::vector<Value> outs = engine().callFunctionHandleMulti(
        handleFor(name), numkit::Span<const Value>(a.data(), a.size()), nout);

    for (size_t k = 1; k < nargout && extra_outs; ++k)
        extra_outs[k - 1] = wrap(new Value(k < outs.size() ? outs[k] : Value()));
    return wrap(new Value(outs.empty() ? Value() : outs[0]));
}

double nk_unbox_scalar(nk_val v) { return unwrap(v)->toScalar(); }

void nk_unbox_array(nk_val v, double *out, size_t len)
{
    const Value  *val = unwrap(v);
    const double *d   = val->doubleData();
    const size_t  nm  = val->numel();
    for (size_t i = 0; i < len && i < nm; ++i) out[i] = d[i];
}

size_t nk_numel(nk_val v) { return unwrap(v)->numel(); }

void nk_release(nk_val v) { delete unwrap(v); }

} // extern "C"
