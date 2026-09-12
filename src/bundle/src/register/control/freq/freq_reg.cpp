// toolboxes/control/src/freq/freq_reg.cpp
//
// Register half of the frequency-response builtins: the CallContext wrappers
// evalfr / freqresp / bode / nyquist / rlocus that delegate to the engine-free
// compute in freq.cpp. library.cpp forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <nk/control/freq/freq.hpp>

#include <nk/core/engine.hpp>   // CallContext, ctx.engine->resource()
#include <nk/value/value.hpp>   // Value::matrix, ValueType (default w/k args)
#include <nk/value/error.hpp>

#include <utility>

namespace nk::control {
namespace detail {

void evalfr_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.size() < 2)
        throw Error("evalfr: requires (sys, f)",
                    0, 0, "evalfr", "", "nk:evalfr:nargin");
    o[0] = evalfr(a[0], a[1].toScalar(), c.engine->resource());
}

void freqresp_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.size() < 2)
        throw Error("freqresp: requires (sys, w)",
                    0, 0, "freqresp", "", "nk:freqresp:nargin");
    o[0] = freqresp(a[0], a[1], c.engine->resource());
}

void bode_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.empty())
        throw Error("bode: requires (sys [, w])",
                    0, 0, "bode", "", "nk:bode:nargin");
    Value wArg = (a.size() >= 2) ? a[1]
                : Value::matrix(0, 0, ValueType::DOUBLE, c.engine->resource());
    auto b = bode(a[0], wArg, c.engine->resource());
    if (o.size() >= 1) o[0] = std::move(b.mag);
    if (o.size() >= 2) o[1] = std::move(b.phase);
    if (o.size() >= 3) o[2] = std::move(b.w);
}

void nyquist_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.empty())
        throw Error("nyquist: requires (sys [, w])",
                    0, 0, "nyquist", "", "nk:nyquist:nargin");
    Value wArg = (a.size() >= 2) ? a[1]
                : Value::matrix(0, 0, ValueType::DOUBLE, c.engine->resource());
    auto n = nyquist(a[0], wArg, c.engine->resource());
    if (o.size() >= 1) o[0] = std::move(n.re);
    if (o.size() >= 2) o[1] = std::move(n.im);
    if (o.size() >= 3) o[2] = std::move(n.w);
}

void rlocus_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.empty())
        throw Error("rlocus: requires (sys [, k])",
                    0, 0, "rlocus", "", "nk:rlocus:nargin");
    Value kArg = (a.size() >= 2) ? a[1]
                : Value::matrix(0, 0, ValueType::DOUBLE, c.engine->resource());
    auto [r, k] = rlocus(a[0], kArg, c.engine->resource());
    if (o.size() >= 1) o[0] = std::move(r);
    if (o.size() >= 2) o[1] = std::move(k);
}

} // namespace detail
} // namespace nk::control
