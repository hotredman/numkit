// libs/wavelet/src/shape/shape_reg.cpp
//
// Register half of the continuous-wavelet shape primitives: the CallContext
// builtins mexihat / morlet / meyeraux / shanwavf / cmorwavf / fbspwavf
// (argument parsing + multi-output psi/x packing) that delegate to the
// engine-free compute in shape.cpp. shape_grid_reg is the shared
// register-side dispatcher. library.cpp forward-declares + registers these.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/wavelet/shape/shape.hpp>

#include <numkit/core/engine.hpp>   // CallContext, ctx.engine->resource()
#include <numkit/value/error.hpp>

#include <string>
#include <tuple>
#include <utility>

namespace numkit::wavelet {
namespace detail {

static void shape_grid_reg(const char *fn,
                           std::tuple<Value, Value> (*impl)(double, double, size_t, std::pmr::memory_resource *),
                           Span<const Value> args, size_t nargout,
                           Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error(std::string(fn) + ": requires (LB, UB, N)",
                    0, 0, fn, "", "numkit:wav:nargin");
    const double lb = args[0].toScalar();
    const double ub = args[1].toScalar();
    const double Nd = args[2].toScalar();
    if (!(Nd >= 0.0))
        throw Error(std::string(fn) + ": N must be non-negative",
                    0, 0, fn, "", "numkit:wav:N");
    const size_t N = static_cast<size_t>(Nd);
    auto [psi, x] = impl(lb, ub, N, ctx.engine->resource());
    outs[0] = std::move(psi);
    if (nargout > 1) outs[1] = std::move(x);
}

void mexihat_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    shape_grid_reg("mexihat", &mexihat, args, nargout, outs, ctx);
}

void morlet_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    shape_grid_reg("morlet", &morlet, args, nargout, outs, ctx);
}

void meyeraux_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("meyeraux: requires x",
                    0, 0, "meyeraux", "", "numkit:meyeraux:nargin");
    outs[0] = meyeraux(args[0], ctx.engine->resource());
}

void shanwavf_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 5)
        throw Error("shanwavf: requires (LB, UB, N, fb, fc)",
                    0, 0, "shanwavf", "", "numkit:shanwavf:nargin");
    const double lb = args[0].toScalar();
    const double ub = args[1].toScalar();
    const size_t N  = static_cast<size_t>(args[2].toScalar());
    const double fb = args[3].toScalar();
    const double fc = args[4].toScalar();
    auto [psi, x] = shanwavf(lb, ub, N, fb, fc, ctx.engine->resource());
    outs[0] = std::move(psi);
    if (nargout > 1) outs[1] = std::move(x);
}

void cmorwavf_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("cmorwavf: requires (LB, UB, N[, fb, fc])",
                    0, 0, "cmorwavf", "", "numkit:cmorwavf:nargin");
    const double lb = args[0].toScalar();
    const double ub = args[1].toScalar();
    const size_t N  = static_cast<size_t>(args[2].toScalar());
    // MATLAB R2025b defaults when only 3 args supplied: fb = fc = 1.
    const double fb = (args.size() >= 4) ? args[3].toScalar() : 1.0;
    const double fc = (args.size() >= 5) ? args[4].toScalar() : 1.0;
    auto [psi, x] = cmorwavf(lb, ub, N, fb, fc, ctx.engine->resource());
    outs[0] = std::move(psi);
    if (nargout > 1) outs[1] = std::move(x);
}

void fbspwavf_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 6)
        throw Error("fbspwavf: requires (LB, UB, N, m, fb, fc)",
                    0, 0, "fbspwavf", "", "numkit:fbspwavf:nargin");
    const double lb = args[0].toScalar();
    const double ub = args[1].toScalar();
    const size_t N  = static_cast<size_t>(args[2].toScalar());
    const int    m  = static_cast<int>(args[3].toScalar());
    const double fb = args[4].toScalar();
    const double fc = args[5].toScalar();
    auto [psi, x] = fbspwavf(lb, ub, N, m, fb, fc, ctx.engine->resource());
    outs[0] = std::move(psi);
    if (nargout > 1) outs[1] = std::move(x);
}

} // namespace detail
} // namespace numkit::wavelet
