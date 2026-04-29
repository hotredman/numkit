// libs/signal/src/measurements/findpeaks.cpp
//
// findpeaks — strict local maxima. Split from libs/signal/src/.

#include <numkit/signal/measurements/findpeaks.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch_arena.hpp>
#include <numkit/core/types.hpp>

#include <cmath>

namespace numkit::signal {

// Strict local maximum: x[i-1] < x[i] > x[i+1]. NaN is never a peak.
// First and last samples are skipped (no left/right neighbour).
std::tuple<Value, Value>
findpeaks(Allocator &alloc, const Value &x)
{
    ScratchArena scratch(alloc);
    auto peakVals = scratch.vec<double>();
    auto peakIdx  = scratch.vec<std::size_t>();
    const size_t n = x.numel();
    if (n >= 3) {
        const double *p = x.doubleData();
        for (size_t i = 1; i + 1 < n; ++i) {
            const double v = p[i];
            if (std::isnan(v) || std::isnan(p[i - 1]) || std::isnan(p[i + 1]))
                continue;
            if (v > p[i - 1] && v > p[i + 1]) {
                peakVals.push_back(v);
                peakIdx.push_back(i);
            }
        }
    }
    auto vals = Value::matrix(1, peakVals.size(), ValueType::DOUBLE, &alloc);
    auto idxs = Value::matrix(1, peakIdx.size(), ValueType::DOUBLE, &alloc);
    for (size_t i = 0; i < peakVals.size(); ++i) {
        vals.doubleDataMut()[i] = peakVals[i];
        idxs.doubleDataMut()[i] = static_cast<double>(peakIdx[i] + 1);  // 1-based
    }
    return std::make_tuple(std::move(vals), std::move(idxs));
}

namespace detail {

void findpeaks_reg(Span<const Value> args, size_t nargout, Span<Value> outs,
                   CallContext &ctx)
{
    if (args.empty())
        throw Error("findpeaks: requires 1 argument",
                     0, 0, "findpeaks", "", "m:findpeaks:nargin");
    auto [vals, idxs] = findpeaks(ctx.engine->allocator(), args[0]);
    outs[0] = std::move(vals);
    if (nargout > 1) outs[1] = std::move(idxs);
}

} // namespace detail

} // namespace numkit::signal
