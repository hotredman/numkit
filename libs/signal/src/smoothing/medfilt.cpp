// libs/signal/src/smoothing/medfilt.cpp
//
// medfilt1 — sliding-window median. Split from libs/signal/src/.

#include <numkit/signal/smoothing/medfilt.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch_arena.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"

#include <algorithm>

namespace numkit::signal {

// Window length k is centered on each sample. For even k MATLAB places
// the center off by half a sample; we follow MATLAB's convention by
// using floor((k-1)/2) elements on the left and ceil((k-1)/2) on the
// right (matches MATLAB R2024b 'truncate' mode for both odd and even k).
//
// At the boundaries the window is truncated rather than zero-padded,
// so output length always equals input length.
Value medfilt1(Allocator &alloc, const Value &x, size_t k)
{
    if (k == 0)
        throw Error("medfilt1: window length must be >= 1",
                     0, 0, "medfilt1", "", "m:medfilt1:badK");

    const size_t n = x.numel();
    auto r = createLike(x, ValueType::DOUBLE, &alloc);
    if (n == 0) return r;

    const size_t leftHalf  = (k - 1) / 2;
    const size_t rightHalf = k / 2;

    ScratchArena scratch(alloc);
    auto win = scratch.vec<double>();
    win.reserve(k);

    const double *src = x.doubleData();
    double *dst = r.doubleDataMut();
    for (size_t i = 0; i < n; ++i) {
        const size_t lo = (i >= leftHalf) ? (i - leftHalf) : 0;
        const size_t hi = std::min(n, i + rightHalf + 1);
        win.assign(src + lo, src + hi);
        const size_t mid = win.size() / 2;
        std::nth_element(win.begin(), win.begin() + mid, win.end());
        if (win.size() % 2 == 1) {
            dst[i] = win[mid];
        } else {
            const double upper = win[mid];
            const double lower = *std::max_element(win.begin(), win.begin() + mid);
            dst[i] = 0.5 * (lower + upper);
        }
    }
    return r;
}

namespace detail {

void medfilt1_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                  CallContext &ctx)
{
    if (args.empty())
        throw Error("medfilt1: requires at least 1 argument",
                     0, 0, "medfilt1", "", "m:medfilt1:nargin");
    size_t k = 3;
    if (args.size() >= 2 && !args[1].isEmpty())
        k = static_cast<size_t>(args[1].toScalar());
    outs[0] = medfilt1(ctx.engine->allocator(), args[0], k);
}

} // namespace detail

} // namespace numkit::signal
