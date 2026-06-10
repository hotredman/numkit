// toolboxes/signal/src/filter_analysis/predicates_reg.cpp
//
// Register half of the signal predicates builtins: the CallContext wrappers
// delegating to the engine-free compute in predicates.cpp. library.cpp
// forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/signal/filter_analysis/predicates.hpp>
#include <numkit/signal/filter_analysis/frequency_response.hpp>
#include "filter_analysis/predicates_detail.hpp"

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace numkit::signal {

namespace detail {

static Value boolVal(bool b, std::pmr::memory_resource *mr)
{
    auto v = Value::matrix(1, 1, ValueType::LOGICAL, mr);
    v.logicalDataMut()[0] = b ? 1 : 0;
    return v;
}

void isfir_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("isfir: requires at least 1 argument (b)",
                     0, 0, "isfir", "", "numkit:isfir:nargin");
    const bool r = (args.size() >= 2) ? isfir(args[0], args[1]) : isfir(args[0]);
    outs[0] = boolVal(r, ctx.engine->resource());
}

void isstable_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("isstable: requires at least 1 argument (b)",
                     0, 0, "isstable", "", "numkit:isstable:nargin");
    const Value &b = args[0];
    Value a = (args.size() >= 2) ? args[1] : Value::scalar(1.0, ctx.engine->resource());
    outs[0] = boolVal(isstable(b, a, ctx.engine->resource()), ctx.engine->resource());
}

void isminphase_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("isminphase: requires at least 1 argument (b)",
                     0, 0, "isminphase", "", "numkit:isminphase:nargin");
    const Value &b = args[0];
    Value a = (args.size() >= 2) ? args[1] : Value::scalar(1.0, ctx.engine->resource());
    outs[0] = boolVal(isminphase(b, a, ctx.engine->resource()), ctx.engine->resource());
}

void ismaxphase_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("ismaxphase: requires at least 1 argument (b)",
                     0, 0, "ismaxphase", "", "numkit:ismaxphase:nargin");
    const Value &b = args[0];
    Value a = (args.size() >= 2) ? args[1] : Value::scalar(1.0, ctx.engine->resource());
    outs[0] = boolVal(ismaxphase(b, a, ctx.engine->resource()), ctx.engine->resource());
}

void islinphase_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("islinphase: requires at least 1 argument (b)",
                     0, 0, "islinphase", "", "numkit:islinphase:nargin");
    const Value &b = args[0];
    Value a = (args.size() >= 2) ? args[1] : Value::scalar(1.0, ctx.engine->resource());
    outs[0] = boolVal(islinphase(b, a), ctx.engine->resource());
}

void isallpass_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("isallpass: requires (b, a)",
                     0, 0, "isallpass", "", "numkit:isallpass:nargin");
    outs[0] = boolVal(isallpass(args[0], args[1]), ctx.engine->resource());
}

void filtord_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("filtord: requires at least 1 argument (b)",
                     0, 0, "filtord", "", "numkit:filtord:nargin");
    int n = (args.size() >= 2)
        ? filtord(args[0], args[1])
        : filtord(args[0]);
    outs[0] = Value::scalar(static_cast<double>(n), ctx.engine->resource());
}

void firtype_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("firtype: requires 1 argument (b)",
                     0, 0, "firtype", "", "numkit:firtype:nargin");
    outs[0] = Value::scalar(static_cast<double>(firtype(args[0])), ctx.engine->resource());
}

void filternorm_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("filternorm: requires (b, a [, pnorm])",
                     0, 0, "filternorm", "", "numkit:filternorm:nargin");
    double p = 2.0;
    if (args.size() >= 3 && !args[2].isEmpty()) p = args[2].toScalar();
    outs[0] = Value::scalar(filternorm(args[0], args[1], p, ctx.engine->resource()),
                            ctx.engine->resource());
}

} // namespace detail

// ── filtord ────────────────────────────────────────────────────────
int filtord(const Value &b)
{
    auto bv = trimTrailingZeros(b);
    if (bv.empty()) return 0;
    return static_cast<int>(bv.size()) - 1;
}

int filtord(const Value &b, const Value &a)
{
    auto bv = trimTrailingZeros(b);
    auto av = trimTrailingZeros(a);
    const size_t lb = bv.empty() ? 0 : bv.size();
    const size_t la = av.empty() ? 0 : av.size();
    const size_t n  = std::max(lb, la);
    return (n == 0) ? 0 : static_cast<int>(n) - 1;
}

// ── firtype ────────────────────────────────────────────────────────
int firtype(const Value &b)
{
    auto v = trimTrailingZeros(b);
    if (v.size() < 2)
        throw Error("firtype: filter must have at least 2 coefficients",
                     0, 0, "firtype", "", "numkit:firtype:short");
    const bool sym  = isSymmetric(v, +1.0);
    const bool anti = isSymmetric(v, -1.0);
    if (!sym && !anti)
        throw Error("firtype: filter is not (anti)symmetric — not a "
                    "linear-phase FIR",
                     0, 0, "firtype", "", "numkit:firtype:notlinphase");
    const int order = static_cast<int>(v.size()) - 1;  // L = order + 1
    const bool order_even = (order % 2 == 0);
    if (sym)  return order_even ? 1 : 2;
    /* anti */ return order_even ? 3 : 4;
}

// ── filternorm ─────────────────────────────────────────────────────
double filternorm(const Value &b, const Value &a, double pnorm, std::pmr::memory_resource *mr)
{
    constexpr size_t kNpts = 8192;
    auto [H, W] = freqz(b, a, kNpts, mr);
    const Complex *hd = H.complexData();

    if (std::isinf(pnorm)) {
        // L∞: max |H(e^{jw})| over the freqz grid.
        double mx = 0.0;
        for (size_t k = 0; k < kNpts; ++k) {
            const double m = std::abs(hd[k]);
            if (m > mx) mx = m;
        }
        return mx;
    }
    if (pnorm != 2.0)
        throw Error("filternorm: only pnorm = 2 or inf supported",
                     0, 0, "filternorm", "", "numkit:filternorm:p");
    // L2: sqrt((1/π) ∫_0^π |H|² dw) via trapezoidal rule on freqz grid.
    // freqz returns npts samples on [0, π] inclusive — Δw = π/(npts-1).
    double sum = 0.0;
    for (size_t k = 0; k < kNpts; ++k) {
        const double mag2 = std::norm(hd[k]);  // |z|² for complex
        const double w = (k == 0 || k == kNpts - 1) ? 0.5 : 1.0;
        sum += w * mag2;
    }
    return std::sqrt(sum / static_cast<double>(kNpts - 1));
}

} // namespace numkit::signal
