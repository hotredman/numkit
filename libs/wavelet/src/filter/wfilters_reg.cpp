// libs/wavelet/src/filter/wfilters_reg.cpp
//
// Register half of wfilters: the CallContext builtin (name/kind parsing,
// 4-output [Lo_D,Hi_D,Lo_R,Hi_R] vs single 2×Lf matrix forms) that
// delegates to the engine-free compute in wfilters.cpp. library.cpp
// forward-declares + registers this by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/wavelet/filter/wfilters.hpp>

#include <numkit/core/engine.hpp>   // CallContext, ctx.engine->resource()
#include <numkit/value/error.hpp>

#include <string>
#include <utility>

namespace numkit::wavelet {
namespace detail {

static std::string argString(const Value &v) {
    if (!v.isChar() && !v.isString())
        throw Error("wavelet: expected a string argument",
                    0, 0, "", "", "numkit:wavelet:type");
    return v.toString();
}

void wfilters_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                  CallContext &ctx)
{
    if (args.empty())
        throw Error("wfilters: requires the wavelet name",
                    0, 0, "wfilters", "", "numkit:wfilters:nargin");
    const std::string name = argString(args[0]);
    std::string kind;
    if (args.size() >= 2) kind = argString(args[1]);

    auto *mr = ctx.engine->resource();
    auto r = wfilters(name, kind, mr);

    if (kind.empty()) {
        // 4-output form: [Lo_D, Hi_D, Lo_R, Hi_R].
        if (outs.size() >= 1) outs[0] = std::move(r.Lo_D);
        if (outs.size() >= 2) outs[1] = std::move(r.Hi_D);
        if (outs.size() >= 3) outs[2] = std::move(r.Lo_R);
        if (outs.size() >= 4) outs[3] = std::move(r.Hi_R);
        (void)nargout;
    } else {
        // 2026-05-08 wavelet/wfilters fix: single-output form
        // returns a 2×Lf matrix `[a; b]` (per MATLAB R2025b), not two
        // separate row vectors. Pack column-major.
        const Value &a = (kind == "d") ? r.Lo_D
                       : (kind == "r") ? r.Lo_R
                       : (kind == "l") ? r.Lo_D
                                       : r.Hi_D;     // 'h'
        const Value &b = (kind == "d") ? r.Hi_D
                       : (kind == "r") ? r.Hi_R
                       : (kind == "l") ? r.Lo_R
                                       : r.Hi_R;     // 'h'
        const size_t Lf = (size_t)a.numel();
        Value M = Value::matrix(2, Lf, ValueType::DOUBLE, mr);
        double *mp = M.doubleDataMut();
        const double *ap = a.doubleData();
        const double *bp = b.doubleData();
        for (size_t k = 0; k < Lf; ++k) {
            mp[k * 2 + 0] = ap[k];
            mp[k * 2 + 1] = bp[k];
        }
        outs[0] = std::move(M);
    }
}

} // namespace detail
} // namespace numkit::wavelet
