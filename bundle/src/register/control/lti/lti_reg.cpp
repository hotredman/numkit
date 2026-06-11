// toolboxes/control/src/lti/lti_reg.cpp
//
// Register half of the LTI-model builtins: the CallContext wrappers for the
// constructors (tf/zpk/ss/filt/frd), the data extractors (tfdata/zpkdata/
// ssdata/frdata) and ss2ss, all delegating to the engine-free compute in
// lti.cpp. library.cpp forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/control/lti/lti.hpp>

#include <numkit/core/engine.hpp>   // CallContext, ctx.engine->resource()
#include <numkit/value/error.hpp>

#include <cctype>
#include <string>
#include <utility>

namespace numkit::control {
namespace detail {

static double argTs(Span<const Value> args, size_t pos) {
    if (args.size() <= pos || args[pos].isEmpty()) return 0.0;
    return args[pos].toScalar();
}

void tf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
            CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("tf: requires (num, den [, Ts])",
                    0, 0, "tf", "", "numkit:tf:nargin");
    outs[0] = tf(args[0], args[1], argTs(args, 2), ctx.engine->resource());
}

void zpk_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
             CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("zpk: requires (z, p, k [, Ts])",
                    0, 0, "zpk", "", "numkit:zpk:nargin");
    outs[0] = zpk(args[0], args[1], args[2].toScalar(), argTs(args, 3),
                  ctx.engine->resource());
}

void ss_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
            CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("ss: requires (A, B, C, D [, Ts])",
                    0, 0, "ss", "", "numkit:ss:nargin");
    outs[0] = ss(args[0], args[1], args[2], args[3], argTs(args, 4), ctx.engine->resource());
}

void filt_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
              CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("filt: requires (num, den [, Ts])",
                    0, 0, "filt", "", "numkit:filt:nargin");
    // MATLAB default Ts for filt is -1 (unspecified discrete).
    const double Ts = (args.size() >= 3 && !args[2].isEmpty())
                      ? args[2].toScalar() : -1.0;
    outs[0] = filt(args[0], args[1], Ts, ctx.engine->resource());
}

void frd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
             CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("frd: requires (response, frequency [, Ts])",
                    0, 0, "frd", "", "numkit:frd:nargin");
    outs[0] = frd(args[0], args[1], argTs(args, 2), ctx.engine->resource());
}

static bool wantVector(Span<const Value> args, size_t pos) {
    if (args.size() <= pos) return false;
    if (!args[pos].isChar() && !args[pos].isString()) return false;
    std::string s = args[pos].toString();
    for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s == "v" || s == "vector";
}

void tfdata_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                CallContext &ctx)
{
    if (args.empty())
        throw Error("tfdata: requires (sys [, 'v'])",
                    0, 0, "tfdata", "", "numkit:tfdata:nargin");
    auto [num, den] = tfdata(args[0], wantVector(args, 1), ctx.engine->resource());
    outs[0] = std::move(num);
    if (nargout > 1) outs[1] = std::move(den);
}

void zpkdata_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                 CallContext &ctx)
{
    if (args.empty())
        throw Error("zpkdata: requires (sys [, 'v'])",
                    0, 0, "zpkdata", "", "numkit:zpkdata:nargin");
    auto [z, p, k] = zpkdata(args[0], wantVector(args, 1), ctx.engine->resource());
    outs[0] = std::move(z);
    if (nargout > 1) outs[1] = std::move(p);
    if (nargout > 2) outs[2] = std::move(k);
}

void ssdata_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                CallContext &ctx)
{
    if (args.empty())
        throw Error("ssdata: requires (sys)",
                    0, 0, "ssdata", "", "numkit:ssdata:nargin");
    auto [A, B, C, D] = ssdata(args[0], ctx.engine->resource());
    outs[0] = std::move(A);
    if (nargout > 1) outs[1] = std::move(B);
    if (nargout > 2) outs[2] = std::move(C);
    if (nargout > 3) outs[3] = std::move(D);
}

void ss2ss_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("ss2ss: requires (sys, T)",
                    0, 0, "ss2ss", "", "numkit:ss2ss:nargin");
    outs[0] = ss2ss(args[0], args[1], ctx.engine->resource());
}

void frdata_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                CallContext &ctx)
{
    if (args.empty())
        throw Error("frdata: requires (sys [, 'v'])",
                    0, 0, "frdata", "", "numkit:frdata:nargin");
    // 'v' flag accepted for MATLAB compatibility; frdata always returns
    // column vectors regardless (we don't model SISO 1×1×N tensors).
    (void)wantVector(args, 1);
    auto [resp, freq] = frdata(args[0], ctx.engine->resource());
    outs[0] = std::move(resp);
    if (nargout > 1) outs[1] = std::move(freq);
}

} // namespace detail
} // namespace numkit::control
