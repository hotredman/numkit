// toolboxes/signal/src/filter_implementation/conversions_extras_reg.cpp
//
// Register half of the signal conversions_extras builtins: the CallContext wrappers
// delegating to the engine-free compute in conversions_extras.cpp. library.cpp
// forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/signal/filter_implementation/conversions_extras.hpp>

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

void sos2tf_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("sos2tf: requires at least 1 argument (sos)",
                     0, 0, "sos2tf", "", "numkit:sos2tf:nargin");
    const double g = (args.size() >= 2) ? args[1].toScalar() : 1.0;
    auto [b, a] = sos2tf(args[0], g, ctx.engine->resource());
    outs[0] = std::move(b);
    if (nargout > 1) outs[1] = std::move(a);
}

void sos2zp_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("sos2zp: requires at least 1 argument (sos)",
                     0, 0, "sos2zp", "", "numkit:sos2zp:nargin");
    const double g = (args.size() >= 2) ? args[1].toScalar() : 1.0;
    auto [z, p, gain] = sos2zp(args[0], g, ctx.engine->resource());
    outs[0] = std::move(z);
    if (nargout > 1) outs[1] = std::move(p);
    if (nargout > 2) outs[2] = Value::scalar(gain, ctx.engine->resource());
}

void tf2zpk_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("tf2zpk: requires (b, a)",
                     0, 0, "tf2zpk", "", "numkit:tf2zpk:nargin");
    auto [z, p, gain] = tf2zpk(args[0], args[1], ctx.engine->resource());
    outs[0] = std::move(z);
    if (nargout > 1) outs[1] = std::move(p);
    if (nargout > 2) outs[2] = Value::scalar(gain, ctx.engine->resource());
}

void tf2ss_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("tf2ss: requires (b, a)",
                     0, 0, "tf2ss", "", "numkit:tf2ss:nargin");
    auto [A, B, C, D] = tf2ss(args[0], args[1], ctx.engine->resource());
    outs[0] = std::move(A);
    if (nargout > 1) outs[1] = std::move(B);
    if (nargout > 2) outs[2] = std::move(C);
    if (nargout > 3) outs[3] = std::move(D);
}

void ss2tf_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("ss2tf: requires (A, B, C, D)",
                     0, 0, "ss2tf", "", "numkit:ss2tf:nargin");
    const double D = (args[3].numel() == 0) ? 0.0 : args[3].elemAsDouble(0);
    auto [b, a] = ss2tf(args[0], args[1], args[2], D, ctx.engine->resource());
    outs[0] = std::move(b);
    if (nargout > 1) outs[1] = std::move(a);
}

void ss2zp_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("ss2zp: requires (A, B, C, D)",
                     0, 0, "ss2zp", "", "numkit:ss2zp:nargin");
    const double D = (args[3].numel() == 0) ? 0.0 : args[3].elemAsDouble(0);
    auto [z, p, gain] = ss2zp(args[0], args[1], args[2], D, ctx.engine->resource());
    outs[0] = std::move(z);
    if (nargout > 1) outs[1] = std::move(p);
    if (nargout > 2) outs[2] = Value::scalar(gain, ctx.engine->resource());
}

void zp2ss_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("zp2ss: requires (z, p, k)",
                     0, 0, "zp2ss", "", "numkit:zp2ss:nargin");
    auto [A, B, C, D] = zp2ss(args[0], args[1], args[2].toScalar(), ctx.engine->resource());
    outs[0] = std::move(A);
    if (nargout > 1) outs[1] = std::move(B);
    if (nargout > 2) outs[2] = std::move(C);
    if (nargout > 3) outs[3] = std::move(D);
}

void sos2ss_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("sos2ss: requires at least 1 argument (sos)",
                     0, 0, "sos2ss", "", "numkit:sos2ss:nargin");
    const double g = (args.size() >= 2) ? args[1].toScalar() : 1.0;
    auto [A, B, C, D] = sos2ss(args[0], g, ctx.engine->resource());
    outs[0] = std::move(A);
    if (nargout > 1) outs[1] = std::move(B);
    if (nargout > 2) outs[2] = std::move(C);
    if (nargout > 3) outs[3] = std::move(D);
}

void ss2sos_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("ss2sos: requires (A, B, C, D)",
                     0, 0, "ss2sos", "", "numkit:ss2sos:nargin");
    const double D = (args[3].numel() == 0) ? 0.0 : args[3].elemAsDouble(0);
    outs[0] = ss2sos(args[0], args[1], args[2], D, ctx.engine->resource());
}

void ctf2zp_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("ctf2zp: requires (NUM [, DEN [, SV]])",
                    0, 0, "ctf2zp", "", "numkit:ctf2zp:nargin");
    auto *mr = ctx.engine->resource();
    Value DEN = (args.size() >= 2) ? args[1] : Value::scalar(1.0, mr);
    const Value &SV = (args.size() >= 3) ? args[2] : Value::Empty;
    auto [Z, P, k] = ctf2zp(args[0], DEN, SV, mr);
    outs[0] = std::move(Z);
    if (nargout >= 2 && outs.size() >= 2) outs[1] = std::move(P);
    if (nargout >= 3 && outs.size() >= 3) outs[2] = Value::scalar(k, mr);
}

void scaleFilterSections_reg(Span<const Value> args, size_t /*nargout*/,
                              Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("scaleFilterSections: requires (CTFNum, SV)",
                    0, 0, "scaleFilterSections", "",
                    "numkit:scaleFilterSections:nargin");
    outs[0] = scaleFilterSections(args[0], args[1], ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::signal
