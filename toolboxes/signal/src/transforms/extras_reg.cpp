// toolboxes/signal/src/transforms/extras_reg.cpp
//
// CallContext register half of transforms/extras.cpp (Phase 2b compute/register split).
// Engine-coupled glue: marshals CallContext args/outs into the engine-free
// compute API declared in the headers below. See project_layering_refactor.
#include <numkit/core/engine.hpp>
#include <numkit/signal/filter_analysis/unwrap.hpp>      // (used externally; not needed here)
#include <numkit/signal/transforms/extras.hpp>
#include <numkit/signal/transforms/fft.hpp>
#include "extras_detail.hpp"
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

void dftmtx_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("dftmtx: requires N",
                     0, 0, "dftmtx", "", "numkit:dftmtx:nargin");
    outs[0] = dftmtx(static_cast<size_t>(args[0].toScalar()), ctx.engine->resource());
}

void bitrevorder_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("bitrevorder: requires x",
                     0, 0, "bitrevorder", "", "numkit:bitrevorder:nargin");
    auto *mr = ctx.engine->resource();
    outs[0] = bitrevorder(args[0], mr);
    // 2nd output: 1-based index vector I such that Y(k) = X(I(k)).
    // Same permutation applied to (1:N), preserving the input shape.
    if (nargout > 1) {
        const size_t n = args[0].numel();
        if (n == 0) {
            outs[1] = Value::matrix(0, 0, ValueType::DOUBLE, mr);
            return;
        }
        size_t bits = 0;
        for (size_t v = n; v > 1; v >>= 1) ++bits;
        const bool isRow = (args[0].dims().rows() == 1);
        Value I = isRow
                    ? Value::matrix(1, n, ValueType::DOUBLE, mr)
                    : Value::matrix(n, 1, ValueType::DOUBLE, mr);
        double *id = I.doubleDataMut();
        for (size_t i = 0; i < n; ++i) {
            // dst[bitReverse(i, bits)] = i+1 (1-based MATLAB index).
            id[bitReverse(i, bits)] = static_cast<double>(i + 1);
        }
        outs[1] = std::move(I);
    }
}

void dst_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("dst: requires x",
                     0, 0, "dst", "", "numkit:dst:nargin");
    outs[0] = dst(args[0], ctx.engine->resource());
}

void idst_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("idst: requires y",
                     0, 0, "idst", "", "numkit:idst:nargin");
    outs[0] = idst(args[0], ctx.engine->resource());
}

void rceps_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("rceps: requires x",
                     0, 0, "rceps", "", "numkit:rceps:nargin");
    auto *mr = ctx.engine->resource();
    if (nargout >= 2) {
        auto [y, ym] = rcepsMinPhase(args[0], mr);
        outs[0] = std::move(y);
        outs[1] = std::move(ym);
        return;
    }
    outs[0] = rceps(args[0], mr);
}

void cceps_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("cceps: requires x",
                     0, 0, "cceps", "", "numkit:cceps:nargin");
    outs[0] = cceps(args[0], ctx.engine->resource());
}

void icceps_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("icceps: requires c",
                     0, 0, "icceps", "", "numkit:icceps:nargin");
    outs[0] = icceps(args[0], ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::signal
