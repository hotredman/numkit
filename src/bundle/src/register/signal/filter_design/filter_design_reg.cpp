// toolboxes/signal/src/filter_design/filter_design_reg.cpp
//
// CallContext register half of filter_design/filter_design.cpp (Phase 2b compute/register split).
// The detail{{}} *_reg fns were interleaved with compute in the original
// TU; collected here verbatim (reg-side arg parsers nested in their anon
// namespaces ride along). See project_layering_refactor.
#include <nk/core/engine.hpp>
#include <nk/builtin/polyfun.hpp>
#include <nk/builtin/specfun.hpp>
#include <nk/signal/filter_design/filter_design.hpp>
#include <nk/signal/filter_implementation/conversions_extras.hpp>
#include <nk/signal/transforms/fft.hpp>
#include <nk/value/error.hpp>
#include <nk/value/scratch.hpp>
#include <nk/value/value.hpp>
#include <nk/value/error.hpp>
#include <nk/value/scratch.hpp>
#include <nk/value/span.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstring>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace nk::signal {

namespace detail {

void fir1_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("fir1: requires at least 2 arguments",
                     0, 0, "fir1", "", "nk:fir1:nargin");
    const int N = static_cast<int>(args[0].toScalar());
    const double Wn = args[1].toScalar();
    std::string type = "low";
    if (args.size() >= 3 && args[2].isChar())
        type = args[2].toString();

    outs[0] = fir1(N, Wn, type, ctx.engine->resource());
}

void firls_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("firls: requires 3 arguments (N, F, A)",
                    0, 0, "firls", "", "nk:firls:nargin");
    const int N = static_cast<int>(args[0].toScalar());
    outs[0] = firls(N, args[1], args[2], ctx.engine->resource());
}

void fir2_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("fir2: requires (N, F, A)",
                    0, 0, "fir2", "", "nk:fir2:nargin");
    const int N = static_cast<int>(args[0].toScalar());

    // Optional trailing arguments fir2(N,F,A[,npt][,lap][,window]):
    // scalars are npt then lap (in order); a vector argument is the
    // output window.
    Fir2Options opts;
    int scalarsSeen = 0;
    for (size_t i = 3; i < args.size(); ++i) {
        const Value &a = args[i];
        if (a.numel() == 1) {
            if (scalarsSeen == 0)
                opts.npt = static_cast<int>(a.toScalar());
            else if (scalarsSeen == 1)
                opts.lap = static_cast<int>(a.toScalar());
            else
                throw Error("fir2: too many scalar arguments",
                            0, 0, "fir2", "", "nk:fir2:nargin");
            ++scalarsSeen;
        } else {
            opts.window = a;
        }
    }
    outs[0] = fir2(N, args[1], args[2], opts, ctx.engine->resource());
}

} // namespace detail

namespace detail {

void cell2sos_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("cell2sos: requires (C)",
                    0, 0, "cell2sos", "", "nk:cell2sos:nargin");
    auto [S, g] = cell2sos(args[0], ctx.engine->resource());
    outs[0] = std::move(S);
    if (nargout >= 2 && outs.size() >= 2) outs[1] = std::move(g);
}

} // namespace detail

namespace detail {

void firpm_reg(Span<const Value> args, std::size_t nargout, Span<Value> outs,
               CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("firpm: requires (N, F, A[, W])",
                    0, 0, "firpm", "", "nk:firpm:nargin");
    const int N = static_cast<int>(args[0].toScalar());

    // Trailing args: weights vector (numeric) or ftype string. MATLAB
    // accepts them in either order (W [, ftype] or ftype only).
    Value W;
    std::string ftype;
    for (std::size_t i = 3; i < args.size(); ++i) {
        if (args[i].isCell())
            throw Error("firpm: cell-form lgrid argument deferred",
                        0, 0, "firpm", "", "nk:firpm:unsupportedLgrid");
        if (args[i].isChar())
            ftype = args[i].toString();
        else
            W = args[i];
    }

    auto [b, err] = firpm(N, args[1], args[2], W, ftype,
                          ctx.engine->resource());
    outs[0] = std::move(b);
    if (nargout >= 2 && outs.size() >= 2)
        outs[1] = Value::scalar(err, ctx.engine->resource());
}

} // namespace detail

} // namespace nk::signal
