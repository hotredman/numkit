// toolboxes/signal/src/filter_design/filter_design_reg.cpp
//
// CallContext register half of filter_design/filter_design.cpp (Phase 2b compute/register split).
// The detail{{}} *_reg fns were interleaved with compute in the original
// TU; collected here verbatim (reg-side arg parsers nested in their anon
// namespaces ride along). See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/math/poly/polynomials.hpp>  // tf2zp (ZPK 3-output)
#include <numkit/signal/filter_design/filter_design.hpp>
#include <numkit/signal/transforms/fft.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>

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

namespace numkit::signal {

namespace detail {

void butter_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("butter: requires at least 2 arguments",
                     0, 0, "butter", "", "numkit:butter:nargin");
    const int N = static_cast<int>(args[0].toScalar());
    const double Wn = args[1].toScalar();
    std::string type = "low";
    if (args.size() >= 3 && args[2].isChar())
        type = args[2].toString();

    auto [bv, av] = butter(N, Wn, type, ctx.engine->resource());
    if (nargout >= 3) {
        // [z, p, k] = butter(...): digital zero/pole/gain. The denominator
        // is monic so the ZPK gain equals b(1); tf2zp recovers z/p as the
        // roots of b/a (same set MATLAB returns; order may differ).
        auto [z, p, k] = ::numkit::math::tf2zp(bv, av, ctx.engine->resource());
        outs[0] = std::move(z);
        outs[1] = std::move(p);
        outs[2] = std::move(k);
        return;
    }
    outs[0] = std::move(bv);
    if (nargout > 1)
        outs[1] = std::move(av);
}

void fir1_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("fir1: requires at least 2 arguments",
                     0, 0, "fir1", "", "numkit:fir1:nargin");
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
                    0, 0, "firls", "", "numkit:firls:nargin");
    const int N = static_cast<int>(args[0].toScalar());
    outs[0] = firls(N, args[1], args[2], ctx.engine->resource());
}

void fir2_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("fir2: requires (N, F, A)",
                    0, 0, "fir2", "", "numkit:fir2:nargin");
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
                            0, 0, "fir2", "", "numkit:fir2:nargin");
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
                    0, 0, "cell2sos", "", "numkit:cell2sos:nargin");
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
                    0, 0, "firpm", "", "numkit:firpm:nargin");
    const int N = static_cast<int>(args[0].toScalar());

    // Trailing args: weights vector (numeric) or ftype string. MATLAB
    // accepts them in either order (W [, ftype] or ftype only).
    Value W;
    std::string ftype;
    for (std::size_t i = 3; i < args.size(); ++i) {
        if (args[i].isCell())
            throw Error("firpm: cell-form lgrid argument deferred",
                        0, 0, "firpm", "", "numkit:firpm:unsupportedLgrid");
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

} // namespace numkit::signal
