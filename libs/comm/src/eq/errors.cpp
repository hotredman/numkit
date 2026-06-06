// libs/comm/src/eq/errors.cpp
//
// biterr / symerr -- bit and symbol error counts/rates between two
// arrays. MATLAB Communications Toolbox metrics.

#include <numkit/comm/eq/errors.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace numkit::comm {

namespace {

// Count bit differences between two non-negative integers (Hamming
// distance over their binary representations). Treats both as uint64.
inline std::uint64_t hammingBits(std::uint64_t a, std::uint64_t b)
{
    std::uint64_t x = a ^ b;
    std::uint64_t count = 0;
    while (x) { count += (x & 1); x >>= 1; }
    return count;
}

} // anonymous namespace

// ── Public C++ API (see eq/errors.hpp) ────────────────────────────────

std::pair<Value, Value> biterr(const Value &x, const Value &y, int k,
                               std::pmr::memory_resource *mr)
{
    const std::size_t n = x.numel();
    if (y.numel() != n)
        throw Error("biterr: x and y must have the same number of elements",
                    0, 0, "biterr", "", "numkit:biterr:size");

    const double *xd = x.doubleData();
    const double *yd = y.doubleData();

    // Determine k: explicit (>= 1), else smallest covering bit width.
    int k_bits = k;
    if (k_bits < 1) {
        std::uint64_t mx = 0;
        for (std::size_t i = 0; i < n; ++i) {
            const double xv = xd[i], yv = yd[i];
            if (xv < 0.0 || yv < 0.0)
                throw Error("biterr: inputs must be non-negative integers",
                            0, 0, "biterr", "", "numkit:biterr:negative");
            const std::uint64_t xi = static_cast<std::uint64_t>(xv);
            const std::uint64_t yi = static_cast<std::uint64_t>(yv);
            mx = std::max(mx, std::max(xi, yi));
        }
        k_bits = 1;
        while ((static_cast<std::uint64_t>(1) << k_bits) <= mx) ++k_bits;
    }

    std::uint64_t total = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const std::uint64_t xi = static_cast<std::uint64_t>(xd[i]);
        const std::uint64_t yi = static_cast<std::uint64_t>(yd[i]);
        total += hammingBits(xi, yi);
    }
    const double total_bits =
        static_cast<double>(n) * static_cast<double>(k_bits);
    const double ratio = (total_bits > 0.0)
        ? static_cast<double>(total) / total_bits
        : 0.0;
    return {Value::scalar(static_cast<double>(total), mr),
            Value::scalar(ratio, mr)};
}

std::pair<Value, Value> symerr(const Value &x, const Value &y,
                               std::pmr::memory_resource *mr)
{
    const std::size_t n = x.numel();
    if (y.numel() != n)
        throw Error("symerr: x and y must have the same number of elements",
                    0, 0, "symerr", "", "numkit:symerr:size");

    const double *xd = x.doubleData();
    const double *yd = y.doubleData();
    std::uint64_t count = 0;
    for (std::size_t i = 0; i < n; ++i)
        if (xd[i] != yd[i]) ++count;

    const double ratio = (n > 0)
        ? static_cast<double>(count) / static_cast<double>(n)
        : 0.0;
    return {Value::scalar(static_cast<double>(count), mr),
            Value::scalar(ratio, mr)};
}

namespace detail {

// biterr(x, y[, k]): out[0] = total bit count, out[1] = ratio.
void biterr_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("biterr: requires (x, y[, k])",
                    0, 0, "biterr", "", "numkit:biterr:nargin");
    int k = 0;  // 0 -> auto width
    if (args.size() >= 3 && !args[2].isEmpty()) {
        k = static_cast<int>(args[2].toScalar());
        if (k < 1)
            throw Error("biterr: k must be >= 1",
                        0, 0, "biterr", "", "numkit:biterr:k");
    }
    auto [number, ratio] = biterr(args[0], args[1], k, ctx.engine->resource());
    outs[0] = std::move(number);
    if (nargout > 1) outs[1] = std::move(ratio);
}

// symerr(x, y): out[0] = symbol error count, out[1] = ratio.
void symerr_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 2)
        throw Error("symerr: requires (x, y)",
                    0, 0, "symerr", "", "numkit:symerr:nargin");
    auto [count, ratio] = symerr(args[0], args[1], ctx.engine->resource());
    outs[0] = std::move(count);
    if (nargout > 1) outs[1] = std::move(ratio);
}

} // namespace detail

} // namespace numkit::comm
