// libs/signal/src/smoothing/medfilt.cpp
//
// medfilt1 — sliding-window median. Split from libs/signal/src/.

#include <numkit/signal/smoothing/medfilt.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"

#include <algorithm>
#include <cctype>
#include <string>

namespace numkit::signal {

namespace {

// Median-filter one length-n signal (src → dst) with an order-k window.
// MATLAB's window for sample i spans [i-leftHalf .. i+rightHalf] with
// leftHalf = floor(k/2), rightHalf = k-1-leftHalf (so for even k the
// window leans LEFT, matching MATLAB R2025b). With `zeropad` (MATLAB's
// default) out-of-range samples count as 0 and the window is always k
// long; otherwise ('truncate') the window is clipped to the valid range.
void medfilt1Column(const double *src, double *dst, size_t n, size_t k,
                    size_t leftHalf, size_t rightHalf, bool zeropad,
                    std::pmr::memory_resource *mr)
{
    if (n == 0) return;

    // Incrementally maintain a sorted window of the IN-RANGE samples; each
    // step adds ≤1 on the right and drops ≤1 on the left (O(k) per step,
    // cache-friendly). Pad zeros are folded into the median by rank, never
    // materialised.
    ScratchArena scratch(mr);
    auto win = ScratchVec<double>(&scratch);
    win.reserve(k);

    size_t hi = std::min(n, rightHalf + 1);
    size_t lo = 0;
    for (size_t i = 0; i < hi; ++i) {
        const auto pos = std::upper_bound(win.begin(), win.end(), src[i]);
        win.insert(pos, src[i]);
    }

    auto computeMedian = [&]() -> double {
        const size_t m = win.size();                  // in-range count
        if (!zeropad) {
            if (m == 0) return 0.0;
            if (m & 1) return win[m / 2];
            return 0.5 * (win[m / 2 - 1] + win[m / 2]);
        }
        // Full window is k long: m in-range samples + (k-m) pad zeros.
        const size_t nz = k - m;
        const size_t nNeg =
            static_cast<size_t>(std::lower_bound(win.begin(), win.end(), 0.0) - win.begin());
        const size_t nZeroWin =
            static_cast<size_t>(std::upper_bound(win.begin(), win.end(), 0.0) - win.begin()) - nNeg;
        const size_t zerosTot = nz + nZeroWin;
        auto valAt = [&](size_t r) -> double {
            if (r < nNeg)              return win[r];        // a negative
            if (r < nNeg + zerosTot)   return 0.0;           // a zero (pad or in-range)
            return win[r - nz];                              // a positive
        };
        if (k & 1) return valAt(k / 2);
        return 0.5 * (valAt(k / 2 - 1) + valAt(k / 2));
    };

    dst[0] = computeMedian();

    for (size_t i = 1; i < n; ++i) {
        const size_t new_lo = (i >= leftHalf) ? (i - leftHalf) : 0;
        const size_t new_hi = std::min(n, i + rightHalf + 1);
        for (size_t j = hi; j < new_hi; ++j) {
            const auto pos = std::upper_bound(win.begin(), win.end(), src[j]);
            win.insert(pos, src[j]);
        }
        hi = new_hi;
        for (size_t j = lo; j < new_lo; ++j) {
            const auto pos = std::lower_bound(win.begin(), win.end(), src[j]);
            if (pos != win.end() && *pos == src[j]) win.erase(pos);
        }
        lo = new_lo;
        dst[i] = computeMedian();
    }
}

} // namespace

Value medfilt1(const Value &x, size_t k, bool zeropad, std::pmr::memory_resource *mr)
{
    if (k == 0)
        throw Error("medfilt1: window length must be >= 1",
                     0, 0, "medfilt1", "", "numkit:medfilt1:badK");

    const size_t total = x.numel();
    auto r = createLike(x, ValueType::DOUBLE, mr);
    if (total == 0) return r;

    const size_t leftHalf  = k / 2;            // MATLAB even-k window leans left
    const size_t rightHalf = k - 1 - leftHalf;

    const double *src = x.doubleData();
    double *dst = r.doubleDataMut();

    const size_t R = static_cast<size_t>(x.dims().dim(0));
    // Vector / scalar → one signal over every element. True 2-D (or N-D)
    // matrix → median-filter each column (length R) independently, matching
    // MATLAB's operate-along-dim-1 default.
    if (R <= 1 || x.dims().isVector()) {
        medfilt1Column(src, dst, total, k, leftHalf, rightHalf, zeropad, mr);
    } else {
        const size_t ncols = total / R;
        for (size_t c = 0; c < ncols; ++c)
            medfilt1Column(src + c * R, dst + c * R, R, k, leftHalf, rightHalf, zeropad, mr);
    }
    return r;
}

namespace detail {

void medfilt1_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                  CallContext &ctx)
{
    if (args.empty())
        throw Error("medfilt1: requires at least 1 argument",
                     0, 0, "medfilt1", "", "numkit:medfilt1:nargin");
    size_t k = 3;
    if (args.size() >= 2 && !args[1].isEmpty()
        && !args[1].isChar() && !args[1].isString())
        k = static_cast<size_t>(args[1].toScalar());

    // Padding mode: MATLAB default 'zeropad'; a trailing 'truncate' string
    // (medfilt1(x,n,'truncate') or medfilt1(x,n,[],dim,'truncate')) clips
    // the window at the ends instead. (blksz/dim/nanflag args are accepted
    // but ignored for now — noted as a gap.)
    bool zeropad = true;
    for (size_t a = args.size(); a-- > 1;) {
        if (args[a].isChar() || args[a].isString()) {
            std::string s = args[a].toString();
            for (char &c : s) c = static_cast<char>(std::tolower((unsigned char)c));
            if (s == "truncate") zeropad = false;
            else if (s == "zeropad") zeropad = true;
            break;
        }
    }
    outs[0] = medfilt1(args[0], k, zeropad, ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::signal
