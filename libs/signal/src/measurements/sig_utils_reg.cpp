// libs/signal/src/measurements/sig_utils_reg.cpp
//
// CallContext register half of measurements/sig_utils.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/signal/measurements/sig_utils.hpp>
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
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace numkit::signal {

namespace detail {

void seqperiod_reg(Span<const Value> args, size_t nargout,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("seqperiod: requires (x [, tol])",
                    0, 0, "seqperiod", "", "numkit:seqperiod:nargin");
    double tol = 1e-10;
    if (args.size() >= 2) tol = args[1].toScalar();
    auto [p, nr] = seqperiod(args[0], tol, ctx.engine->resource());
    outs[0] = p;
    if (nargout >= 2 && outs.size() >= 2) outs[1] = nr;
}

void zerocrossrate_reg(Span<const Value> args, size_t nargout,
                       Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("zerocrossrate: requires (x [, level])",
                    0, 0, "zerocrossrate", "", "numkit:zerocrossrate:nargin");
    double level = 0.0;
    if (args.size() >= 2) level = args[1].toScalar();
    auto [r, c] = zerocrossrate(args[0], level, ctx.engine->resource());
    outs[0] = r;
    if (nargout >= 2 && outs.size() >= 2) outs[1] = c;
}

void cusum_reg(Span<const Value> args, size_t nargout,
               Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("cusum: requires (x [, climit, mshift, tmean, tdev])",
                    0, 0, "cusum", "", "numkit:cusum:nargin");
    const double climit = (args.size() >= 2) ? args[1].toScalar() : 5.0;
    const double mshift = (args.size() >= 3) ? args[2].toScalar() : 1.0;
    CusumResult R = cusum(args[0], climit, mshift,
                          (args.size() >= 4) ? args[3] : Value::Empty,
                          (args.size() >= 5) ? args[4] : Value::Empty,
                          ctx.engine->resource());
    outs[0] = R.iupper;
    if (nargout >= 2 && outs.size() >= 2) outs[1] = R.ilower;
    if (nargout >= 3 && outs.size() >= 3) outs[2] = R.uppersum;
    if (nargout >= 4 && outs.size() >= 4) outs[3] = R.lowersum;
}

} // namespace detail

} // namespace numkit::signal
