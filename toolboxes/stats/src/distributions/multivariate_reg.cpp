// toolboxes/signal/src/distributions/multivariate_reg.cpp
//
// CallContext register half of distributions/multivariate.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/builtin/math/random/rng.hpp>   // sharedEngine / rngMutex
#include <numkit/builtin/math/special/special.hpp>  // betainc (used by mvtcdf d=1)
#include <numkit/stats/distributions/multivariate.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/value.hpp>
#include "dist_helpers.hpp"
#include "multivariate_detail.hpp"
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <complex>
#include <cstddef>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace numkit::stats {

namespace detail {

void mvncdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("mvncdf: requires (X [, mu, Sigma])",
                    0, 0, "mvncdf", "", "numkit:mvncdf:nargin");
    const Value muV    = (args.size() >= 2) ? args[1] : Value();
    const Value sigmaV = (args.size() >= 3) ? args[2] : Value();
    outs[0] = mvncdf(args[0], muV, sigmaV, ctx.engine->resource());
}

void mvnrnd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("mvnrnd: requires (mu, Sigma[, n])",
                     0, 0, "mvnrnd", "", "numkit:mvnrnd:nargin");
    std::size_t n = 0;
    if (args.size() >= 3) n = static_cast<std::size_t>(args[2].toScalar());
    outs[0] = mvnrnd(args[0], args[1], n, ctx.engine->resource());
}

void mvtrnd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("mvtrnd: requires (C, df, n)",
                    0, 0, "mvtrnd", "", "numkit:mvtrnd:nargin");
    const double df = args[1].toScalar();
    const std::size_t n = static_cast<std::size_t>(args[2].toScalar());
    outs[0] = mvtrnd(args[0], df, n, ctx.engine->resource());
}

void mnrnd_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("mnrnd: requires (N, P [, m])",
                    0, 0, "mnrnd", "", "numkit:mnrnd:nargin");
    const std::size_t N = static_cast<std::size_t>(args[0].toScalar());
    std::size_t m = 1;
    if (args.size() >= 3 && !args[2].isEmpty())
        m = static_cast<std::size_t>(args[2].toScalar());
    outs[0] = mnrnd(N, args[1], m, ctx.engine->resource());
}

void wishrnd_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("wishrnd: requires (Sigma, df[, D])",
                    0, 0, "wishrnd", "", "numkit:wishrnd:nargin");
    const double df = args[1].toScalar();
    const Value D_in = (args.size() > 2) ? args[2] : Value::Empty;
    auto [W, D] = wishrnd_factor(args[0], df, D_in, ctx.engine->resource());
    outs[0] = std::move(W);
    if (nargout >= 2) outs[1] = std::move(D);
}

void iwishrnd_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("iwishrnd: requires (Tau, df[, DI])",
                    0, 0, "iwishrnd", "", "numkit:iwishrnd:nargin");
    const double df = args[1].toScalar();
    const Value DI_in = (args.size() > 2) ? args[2] : Value::Empty;
    auto [W, DI] = iwishrnd_factor(args[0], df, DI_in, ctx.engine->resource());
    outs[0] = std::move(W);
    if (nargout >= 2) outs[1] = std::move(DI);
}

void mvtcdf_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    // 3 args: mvtcdf(X, C, df) — upper-tail form.
    // 4 args: mvtcdf(L, U, C, df) — box form.
    // 5 args: mvtcdf(L, U, C, df, tol) — box form with tolerance.
    if (args.size() == 3) {
        outs[0] = mvtcdf(args[0], args[1], args[2].toScalar(), 0.01, mr);
    } else if (args.size() == 4) {
        outs[0] = mvtcdf_box(args[0], args[1], args[2],
                             args[3].toScalar(), 0.01, mr);
    } else if (args.size() == 5) {
        outs[0] = mvtcdf_box(args[0], args[1], args[2],
                             args[3].toScalar(), args[4].toScalar(), mr);
    } else {
        throw Error("mvtcdf: requires (X, C, df) or (L, U, C, df[, tol])",
                    0, 0, "mvtcdf", "", "numkit:mvtcdf:nargin");
    }
}

} // namespace detail

} // namespace numkit::stats
