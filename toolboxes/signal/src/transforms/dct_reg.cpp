// toolboxes/signal/src/transforms/dct_reg.cpp
//
// CallContext register half of transforms/dct.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/signal/transforms/dct.hpp>
#include <numkit/signal/transforms/fft.hpp>
#include "dct_detail.hpp"
#include <numkit/value/error.hpp>
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

// Helper: detect whether the user passed a length override / dim arg.
// MATLAB syntax: dct(X), dct(X, n), dct(X, n, dim). 'Type' name-value
// is parsed separately (currently not implemented; explicit error to
// surface the unsupported branch instead of silently doing Type-II).
struct DctArgs { int n; int dim; bool hasType; double typeVal; };
static DctArgs parseDctArgs(Span<const Value> args, const char *fn)
{
    DctArgs a{0, 0, false, 2.0};
    size_t pos = 1;
    while (pos < args.size()) {
        if (args[pos].isChar() || args[pos].isString()) {
            if (pos + 1 >= args.size())
                throw Error(std::string(fn) + ": missing value after name",
                             0, 0, fn, "", "numkit:dct:nv");
            const std::string key = args[pos].toString();
            if (key == "Type" || key == "type") {
                a.hasType = true;
                a.typeVal = args[pos + 1].toScalar();
            }
            // Unknown N-V keys are silently ignored.
            pos += 2;
            continue;
        }
        if (pos == 1) a.n = static_cast<int>(args[pos].toScalar());
        else if (pos == 2) a.dim = static_cast<int>(args[pos].toScalar());
        else
            throw Error(std::string(fn) + ": too many positional arguments",
                         0, 0, fn, "", "numkit:dct:nargin");
        ++pos;
    }
    return a;
}

void dct_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
             CallContext &ctx)
{
    if (args.empty())
        throw Error("dct: requires at least 1 argument",
                     0, 0, "dct", "", "numkit:dct:nargin");
    auto *mr = ctx.engine->resource();
    if (args.size() == 1) {
        const auto &x = args[0];
        if (x.dims().rows() > 1 && x.dims().cols() > 1) {
            outs[0] = dct(x, /*n=*/0, /*dim=*/0, mr);  // matrix column-wise
        } else {
            outs[0] = dct(x, mr);  // 1-D fast path
        }
        return;
    }
    auto a = parseDctArgs(args, "dct");
    const double t = a.hasType ? a.typeVal : 2.0;
    if (t != 1.0 && t != 2.0 && t != 3.0 && t != 4.0)
        throw Error("dct: 'Type' must be 1, 2, 3, or 4",
                     0, 0, "dct", "", "numkit:dct:type");
    if (t == 2.0) outs[0] = dct(args[0], a.n, a.dim, mr);
    else          outs[0] = dctTyped(args[0], a.n, a.dim, t, /*inverse=*/false, mr);
}

void idct_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
              CallContext &ctx)
{
    if (args.empty())
        throw Error("idct: requires at least 1 argument",
                     0, 0, "idct", "", "numkit:idct:nargin");
    auto *mr = ctx.engine->resource();
    if (args.size() == 1) {
        const auto &x = args[0];
        if (x.dims().rows() > 1 && x.dims().cols() > 1) {
            outs[0] = idct(x, /*n=*/0, /*dim=*/0, mr);
        } else {
            outs[0] = idct(x, mr);
        }
        return;
    }
    auto a = parseDctArgs(args, "idct");
    const double t = a.hasType ? a.typeVal : 2.0;
    if (t != 1.0 && t != 2.0 && t != 3.0 && t != 4.0)
        throw Error("idct: 'Type' must be 1, 2, 3, or 4",
                     0, 0, "idct", "", "numkit:idct:type");
    if (t == 2.0) outs[0] = idct(args[0], a.n, a.dim, mr);
    else          outs[0] = dctTyped(args[0], a.n, a.dim, t, /*inverse=*/true, mr);
}

} // namespace detail

} // namespace numkit::signal
