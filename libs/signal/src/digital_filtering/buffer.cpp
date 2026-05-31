// libs/signal/src/digital_filtering/buffer.cpp
//
// MATLAB R2025b Signal Toolbox `buffer` — frame partitioning utility.
//
// Variants supported:
//   Y = buffer(X, N)              non-overlapping, last frame zero-padded
//   Y = buffer(X, N, P)           P>0: overlap (initial P zeros prepended)
//                                  P<0: underlap (skip |P|/frame)
//   Y = buffer(X, N, P, 'nodelay')  P>0: overlap WITHOUT initial zeros
//   Y = buffer(X, N, P, OFFSET)     P<0: numeric offset (initial samples skipped)
//
// Output: Y is N × numFrames (column-major), one frame per column.
//
// 2-output form [Y, Z] = buffer(...) splits complete frames into Y and
// the trailing partial-frame samples into Z. Z preserves the input's
// row/column orientation.
//
// PMR HARD RULE.

#include <numkit/signal/digital_filtering/buffer.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace numkit::signal {

namespace {

// Detect orientation of x (1=row vector, 2=column vector). For matrix
// inputs MATLAB buffer expects vectors, but we treat any 2D input by
// flattening linearly.
int rowOrCol(const Value &x)
{
    return (x.dims().rows() == 1 && x.dims().cols() > 1) ? 1 : 2;
}

bool isStringLikeNoDelay(const Value &v)
{
    if (v.type() != ValueType::CHAR && v.type() != ValueType::STRING) return false;
    std::string s = v.toString();
    std::transform(s.begin(), s.end(), s.begin(),
                    [](unsigned char c) { return std::tolower(c); });
    return s == "nodelay";
}

// Core buffering algorithm. Returns Y, plus optional partialCount and
// startIndexOfPartial (relative to original x).
struct BufferResult {
    Value Y;
    size_t partialStart;  // index into x where partial-frame Z starts (incl)
    size_t partialEnd;    // index into x where partial-frame Z ends (excl)
};

BufferResult bufferCore(const Value &x, int n, int p, const Value &opt, bool forceCompleteOnly /* true if [Y, Z] form */, std::pmr::memory_resource *mr)
{
    if (n <= 0) {
        BufferResult r;
        r.Y = Value::matrix(0, 0, ValueType::DOUBLE, mr);
        r.partialStart = 0;
        r.partialEnd = 0;
        return r;
    }
    const size_t N = x.numel();
    const size_t Nn = static_cast<size_t>(n);
    const int hop = n - p;  // can be negative when p > n (invalid)

    // Underlap branch (p < 0): skip |p| samples per frame.
    if (p < 0) {
        // Initial offset in [0, -p].
        size_t offset = 0;
        if (!opt.isEmpty()) {
            const double v = opt.toScalar();
            if (v < 0.0 || v > -p)
                throw Error("buffer: OPT (offset) for underlap must be in [0, -P]",
                            0, 0, "buffer", "", "numkit:buffer:BadOpt");
            offset = static_cast<size_t>(v);
        }
        const size_t startN = (offset < N) ? (N - offset) : 0;
        const size_t step = static_cast<size_t>(hop);  // n + |p|
        // Number of frames that fit
        size_t numFrames = 0;
        if (startN >= Nn) numFrames = (startN - Nn) / step + 1;
        else if (!forceCompleteOnly && startN > 0) numFrames = 1;  // partial → padded
        BufferResult r;
        r.Y = Value::matrix(Nn, numFrames, ValueType::DOUBLE, mr);
        if (numFrames == 0) {
            // [Y, Z]: Z = x(offset+1 : end) (the unframed remainder)
            r.partialStart = offset;
            r.partialEnd = N;
            return r;
        }
        double *yd = r.Y.doubleDataMut();
        std::fill(yd, yd + Nn * numFrames, 0.0);
        for (size_t f = 0; f < numFrames; ++f) {
            const size_t base = offset + f * step;
            for (size_t i = 0; i < Nn; ++i) {
                if (base + i < N) yd[i + f * Nn] = x.elemAsDouble(base + i);
            }
        }
        // Trailing partial samples for [Y, Z] form
        const size_t lastConsumed = offset + (numFrames - 1) * step + Nn;
        r.partialStart = (lastConsumed < N) ? lastConsumed : N;
        r.partialEnd = N;
        return r;
    }

    // Overlap (p > 0) and zero-overlap (p == 0) branches.
    const size_t hopP = static_cast<size_t>(hop);  // = n - p, must be > 0
    if (hopP == 0)
        throw Error("buffer: P must be < N (overlap < frame length)",
                    0, 0, "buffer", "", "numkit:buffer:BadOverlap");

    // Decide whether to apply initial-condition prepend (p > 0 only).
    bool prependZeros = (p > 0);
    size_t initLen = (p > 0) ? static_cast<size_t>(p) : 0;
    std::vector<double> initVec(initLen, 0.0);  // initial condition (zeros default)

    if (p > 0 && !opt.isEmpty()) {
        if (isStringLikeNoDelay(opt)) {
            prependZeros = false;
            initLen = 0;
            initVec.clear();
        } else {
            // Numeric initial-condition vector of length p.
            if (opt.numel() != static_cast<size_t>(p))
                throw Error("buffer: initial-condition OPT must have length P",
                            0, 0, "buffer", "", "numkit:buffer:BadInitLen");
            initVec.resize(p);
            for (size_t i = 0; i < static_cast<size_t>(p); ++i)
                initVec[i] = opt.elemAsDouble(i);
        }
    }

    // Effective input length includes the prepended initial samples.
    const size_t effN = N + initLen;

    // Number of complete frames: floor((effN - n) / hop) + 1
    size_t numComplete = 0;
    if (effN >= Nn) numComplete = (effN - Nn) / hopP + 1;

    // Number of frames in 1-output form (with zero-pad) — total frames
    // produced after potentially padding the last frame.
    // MATLAB: numFrames = ceil((effN - p) / hop) when p > 0,
    //         else ceil(effN / n)
    // Simpler: if effN > lastConsumed, add one frame.
    size_t numFramesOneOut = numComplete;
    const size_t lastConsumed = numComplete * hopP + (numComplete > 0 ? p : 0);
    // If there's leftover input, one more frame (zero-padded).
    if (lastConsumed < effN) ++numFramesOneOut;
    // Edge case: effN > 0 but no complete frames yet → still one (padded).
    if (numComplete == 0 && effN > 0) numFramesOneOut = 1;

    const size_t numFramesOut = forceCompleteOnly ? numComplete : numFramesOneOut;
    BufferResult r;
    r.Y = Value::matrix(Nn, numFramesOut, ValueType::DOUBLE, mr);
    double *yd = r.Y.doubleDataMut();
    std::fill(yd, yd + Nn * numFramesOut, 0.0);

    // Index helper into "effective" input (initVec || x).
    auto effGet = [&](size_t idx) -> double {
        if (idx < initLen) return initVec[idx];
        const size_t xi = idx - initLen;
        return (xi < N) ? x.elemAsDouble(xi) : 0.0;
    };

    for (size_t f = 0; f < numFramesOut; ++f) {
        const size_t base = f * hopP;
        for (size_t i = 0; i < Nn; ++i)
            yd[i + f * Nn] = effGet(base + i);
    }

    // Z (partial) = x samples beyond what numComplete frames consumed.
    const size_t lastConsumedComplete =
        (numComplete > 0) ? (numComplete - 1) * hopP + Nn : 0;
    // Convert to original-x index: subtract initLen if positive.
    size_t zStart = (lastConsumedComplete > initLen)
                     ? (lastConsumedComplete - initLen)
                     : 0;
    if (numComplete == 0) zStart = 0;  // all of x is partial
    r.partialStart = std::min(zStart, N);
    r.partialEnd = N;
    return r;
}

Value makePartialZ(const Value &x, size_t startIdx, size_t endIdx, int xOrient, std::pmr::memory_resource *mr)
{
    const size_t len = (endIdx > startIdx) ? (endIdx - startIdx) : 0;
    const size_t rows = (xOrient == 1) ? 1 : len;
    const size_t cols = (xOrient == 1) ? len : (len == 0 ? 0 : 1);
    Value z = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (len == 0) return z;
    double *zd = z.doubleDataMut();
    for (size_t i = 0; i < len; ++i)
        zd[i] = x.elemAsDouble(startIdx + i);
    return z;
}

} // anon

Value buffer(const Value &x, int n, int p, const Value &opt, std::pmr::memory_resource *mr)
{
    BufferResult r = bufferCore(x, n, p, opt, /*forceCompleteOnly=*/false, mr);
    return r.Y;
}

std::tuple<Value, Value>
buffer2(const Value &x, int n, int p, const Value &opt, std::pmr::memory_resource *mr)
{
    BufferResult r = bufferCore(x, n, p, opt, /*forceCompleteOnly=*/true, mr);
    Value z = makePartialZ(x, r.partialStart, r.partialEnd, rowOrCol(x), mr);
    return {r.Y, z};
}

namespace detail {

void buffer_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("buffer: requires (x, n [, p [, opt]])",
                    0, 0, "buffer", "", "numkit:buffer:nargin");
    const int n = static_cast<int>(args[1].toScalar());
    int p = 0;
    if (args.size() >= 3 && !args[2].isEmpty()) p = static_cast<int>(args[2].toScalar());
    const Value &opt = (args.size() >= 4) ? args[3] : Value::Empty;
    if (nargout >= 2 && outs.size() >= 2) {
        auto [Y, Z] = buffer2(args[0], n, p, opt, ctx.engine->resource());
        outs[0] = Y;
        outs[1] = Z;
    } else {
        outs[0] = buffer(args[0], n, p, opt, ctx.engine->resource());
    }
}

} // namespace detail

} // namespace numkit::signal
