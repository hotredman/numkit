// libs/comm/src/eq/errors.cpp
//
// biterr / symerr -- bit and symbol error counts/rates between two
// arrays. MATLAB Communications Toolbox metrics.

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include <cmath>
#include <cstdint>

namespace numkit::comm::detail {

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

// biterr(x, y[, k]):
//   Counts differing bits between x and y. Both arrays must have same
//   shape. Each element is interpreted as a non-negative integer over
//   k bits (default: smallest covering width). Returns:
//     out[0] = total bit count
//     out[1] = ratio = total / (numel * k)
void biterr_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("biterr: requires (x, y[, k])",
                    0, 0, "biterr", "", "numkit:biterr:nargin");
    auto *mr = ctx.engine->resource();
    const Value &X = args[0];
    const Value &Y = args[1];
    const std::size_t n = X.numel();
    if (Y.numel() != n)
        throw Error("biterr: x and y must have the same number of elements",
                    0, 0, "biterr", "", "numkit:biterr:size");

    const double *xd = X.doubleData();
    const double *yd = Y.doubleData();

    // Determine k: explicit, else smallest covering bit width.
    int k_bits = 1;
    if (args.size() >= 3 && !args[2].isEmpty()) {
        k_bits = static_cast<int>(args[2].toScalar());
        if (k_bits < 1)
            throw Error("biterr: k must be >= 1",
                        0, 0, "biterr", "", "numkit:biterr:k");
    } else {
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
        if (k_bits == 1 && mx == 0) k_bits = 1;  // all zeros: single bit
    }

    std::uint64_t total = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const std::uint64_t xi = static_cast<std::uint64_t>(xd[i]);
        const std::uint64_t yi = static_cast<std::uint64_t>(yd[i]);
        total += hammingBits(xi, yi);
    }
    const double total_bits = static_cast<double>(n) * static_cast<double>(k_bits);
    outs[0] = Value::scalar(static_cast<double>(total), mr);
    if (nargout > 1) {
        const double ratio = (total_bits > 0.0)
            ? static_cast<double>(total) / total_bits
            : 0.0;
        outs[1] = Value::scalar(ratio, mr);
    }
}

// symerr(x, y):
//   Counts symbol differences between x and y (element-wise inequality).
//   Returns (count, ratio).
void symerr_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() != 2)
        throw Error("symerr: requires (x, y)",
                    0, 0, "symerr", "", "numkit:symerr:nargin");
    auto *mr = ctx.engine->resource();
    const Value &X = args[0];
    const Value &Y = args[1];
    const std::size_t n = X.numel();
    if (Y.numel() != n)
        throw Error("symerr: x and y must have the same number of elements",
                    0, 0, "symerr", "", "numkit:symerr:size");

    const double *xd = X.doubleData();
    const double *yd = Y.doubleData();
    std::uint64_t count = 0;
    for (std::size_t i = 0; i < n; ++i)
        if (xd[i] != yd[i]) ++count;

    outs[0] = Value::scalar(static_cast<double>(count), mr);
    if (nargout > 1) {
        const double ratio = (n > 0)
            ? static_cast<double>(count) / static_cast<double>(n)
            : 0.0;
        outs[1] = Value::scalar(ratio, mr);
    }
}

} // namespace numkit::comm::detail
