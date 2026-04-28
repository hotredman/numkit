// libs/signal/src/measurements/findpeaks.cpp
//
// findpeaks — strict local maxima. Split from MDspGaps.

#include <numkit/m/signal/measurements/findpeaks.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include <cmath>
#include <vector>

namespace numkit::m::signal {

// Strict local maximum: x[i-1] < x[i] > x[i+1]. NaN is never a peak.
// First and last samples are skipped (no left/right neighbour).
std::tuple<MValue, MValue>
findpeaks(Allocator &alloc, const MValue &x)
{
    std::vector<double> peakVals;
    std::vector<size_t> peakIdx;
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
    auto vals = MValue::matrix(1, peakVals.size(), MType::DOUBLE, &alloc);
    auto idxs = MValue::matrix(1, peakIdx.size(), MType::DOUBLE, &alloc);
    for (size_t i = 0; i < peakVals.size(); ++i) {
        vals.doubleDataMut()[i] = peakVals[i];
        idxs.doubleDataMut()[i] = static_cast<double>(peakIdx[i] + 1);  // 1-based
    }
    return std::make_tuple(std::move(vals), std::move(idxs));
}

namespace detail {

void findpeaks_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs,
                   CallContext &ctx)
{
    if (args.empty())
        throw MError("findpeaks: requires 1 argument",
                     0, 0, "findpeaks", "", "m:findpeaks:nargin");
    auto [vals, idxs] = findpeaks(ctx.engine->allocator(), args[0]);
    outs[0] = std::move(vals);
    if (nargout > 1) outs[1] = std::move(idxs);
}

} // namespace detail

} // namespace numkit::m::signal
