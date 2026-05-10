// libs/signal/src/measurements/sig_utils.cpp
//
// Signal Processing Toolbox small utilities (cycle 5):
//
//   seqperiod(x [, tol])           Smallest period d ≤ N such that
//                                  x(i) ≈ x(((i-1) mod d) + 1) for all i.
//                                  Returns [p, nr] where nr = N/p.
//   zerocrossrate(x [, level])     Zero-crossing rate counting boundary
//                                  half-crossings. count = #crossings
//                                  + 0.5; rate = count / numel(x).
//                                  v1: vector input only, default
//                                  level=0, no Name=Value args.
//   cusum(x [, climit, mshift, tmean, tdev])
//                                  CUSUM control-chart change detector.
//                                  Returns [iupper, ilower] of first
//                                  out-of-control indices. v1 covers
//                                  [iupper, ilower] form (4-out form
//                                  with sums also supported).
//
// PMR HARD RULE: every fn takes std::pmr::memory_resource *mr.
//
// KNOWN GAPs:
//   * zerocrossrate: matrix/N-D input + Name=Value args
//     ('Threshold', 'TransitionEdge', 'WindowLength') deferred.
//   * cusum: 'all' string flag and the no-output plotting form
//     deferred.
//   * seqperiod: matrix/N-D operates column-wise — only vector
//     supported in v1.

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>
#include <numkit/core/value.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace numkit::signal {

// ── seqperiod ─────────────────────────────────────────────────────────
std::pair<Value, Value>
seqperiod(std::pmr::memory_resource *mr, const Value &x, double tol)
{
    const size_t N = x.numel();
    if (N == 0) {
        return {Value::scalar(0.0, mr), Value::scalar(0.0, mr)};
    }
    // Find smallest d such that |x(i) - x(((i-1) mod d) + 1)| ≤ tol for all i.
    size_t period = N;
    for (size_t d = 1; d <= N; ++d) {
        bool ok = true;
        for (size_t i = d; i < N; ++i) {
            const double dv = std::abs(x.elemAsDouble(i) - x.elemAsDouble(i % d));
            if (dv > tol) { ok = false; break; }
        }
        if (ok) { period = d; break; }
    }
    return {Value::scalar(static_cast<double>(period), mr),
            Value::scalar(static_cast<double>(N) / static_cast<double>(period), mr)};
}

// ── zerocrossrate ─────────────────────────────────────────────────────
// Count sign changes relative to `level` (default 0); add 0.5 boundary
// half-credit (matches MATLAB R2025b default ZeroPositive=false).
std::pair<Value, Value>
zerocrossrate(std::pmr::memory_resource *mr, const Value &x, double level)
{
    const size_t N = x.numel();
    if (N <= 1) {
        // Edge: no transitions possible; rate = 0.5 / N (boundary credit).
        const double cnt = (N == 0) ? 0.0 : 0.5;
        return {Value::scalar(N == 0 ? 0.0 : cnt / static_cast<double>(N), mr),
                Value::scalar(cnt, mr)};
    }
    long long crossings = 0;
    double prev = x.elemAsDouble(0) - level;
    int prev_sign = (prev > 0.0) ? 1 : (prev < 0.0 ? -1 : 0);
    for (size_t i = 1; i < N; ++i) {
        const double v = x.elemAsDouble(i) - level;
        const int s = (v > 0.0) ? 1 : (v < 0.0 ? -1 : 0);
        // Count any flip including via zero (treat zero as continuation,
        // matching MATLAB ZeroPositive=false: zero stays at previous sign).
        if (s != 0 && prev_sign != 0 && s != prev_sign) ++crossings;
        if (s != 0) prev_sign = s;
    }
    const double count = static_cast<double>(crossings) + 0.5;
    const double rate  = count / static_cast<double>(N);
    return {Value::scalar(rate, mr), Value::scalar(count, mr)};
}

// ── cusum ─────────────────────────────────────────────────────────────
// Standard CUSUM detector. Returns first indices where cumulative sum
// exceeds the climit threshold (in standard-deviation units).
struct CusumResult {
    Value iupper;     // scalar (first index) or empty if no detection
    Value ilower;
    Value uppersum;   // length-N vector
    Value lowersum;
};

CusumResult cusum_impl(std::pmr::memory_resource *mr, const Value &x,
                       double climit, double mshift,
                       bool have_tmean, double tmean_user,
                       bool have_tdev,  double tdev_user)
{
    const size_t N = x.numel();
    CusumResult R;
    R.uppersum = Value::matrix(N, N == 0 ? 0 : 1, ValueType::DOUBLE, mr);
    R.lowersum = Value::matrix(N, N == 0 ? 0 : 1, ValueType::DOUBLE, mr);
    R.iupper = Value::matrix(0, 0, ValueType::DOUBLE, mr);
    R.ilower = Value::matrix(0, 0, ValueType::DOUBLE, mr);
    if (N == 0) return R;

    // Defaults: tmean = mean(x(1:25)), tdev = std(x(1:25)).
    const size_t baseN = std::min<size_t>(25, N);
    double tmean = tmean_user;
    double tdev  = tdev_user;
    if (!have_tmean) {
        double sum = 0.0;
        for (size_t i = 0; i < baseN; ++i) sum += x.elemAsDouble(i);
        tmean = sum / static_cast<double>(baseN);
    }
    if (!have_tdev) {
        double sum = 0.0, sumsq = 0.0;
        for (size_t i = 0; i < baseN; ++i) {
            const double v = x.elemAsDouble(i);
            sum += v; sumsq += v * v;
        }
        const double m = sum / static_cast<double>(baseN);
        const double var = (sumsq - static_cast<double>(baseN) * m * m)
                          / static_cast<double>(baseN > 1 ? baseN - 1 : 1);
        tdev = std::sqrt(std::max(0.0, var));
    }
    if (tdev <= 0.0) tdev = 1.0;  // guard

    double *us = R.uppersum.doubleDataMut();
    double *ls = R.lowersum.doubleDataMut();
    double up = 0.0, lo = 0.0;
    long long iup_first = -1, ilo_first = -1;
    const double half_shift = 0.5 * mshift;
    for (size_t i = 0; i < N; ++i) {
        const double z = (x.elemAsDouble(i) - tmean) / tdev;
        up = std::max(0.0, up + z - half_shift);
        lo = std::max(0.0, lo - z - half_shift);
        us[i] = up;
        ls[i] = lo;
        if (iup_first < 0 && up > climit) iup_first = static_cast<long long>(i + 1);
        if (ilo_first < 0 && lo > climit) ilo_first = static_cast<long long>(i + 1);
    }
    if (iup_first > 0) R.iupper = Value::scalar(static_cast<double>(iup_first), mr);
    if (ilo_first > 0) R.ilower = Value::scalar(static_cast<double>(ilo_first), mr);
    return R;
}

namespace detail {

void seqperiod_reg(Span<const Value> args, size_t nargout,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("seqperiod: requires (x [, tol])",
                    0, 0, "seqperiod", "", "m:seqperiod:nargin");
    double tol = 1e-10;
    if (args.size() >= 2) tol = args[1].toScalar();
    auto [p, nr] = seqperiod(ctx.engine->resource(), args[0], tol);
    outs[0] = p;
    if (nargout >= 2 && outs.size() >= 2) outs[1] = nr;
}

void zerocrossrate_reg(Span<const Value> args, size_t nargout,
                       Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("zerocrossrate: requires (x [, level])",
                    0, 0, "zerocrossrate", "", "m:zerocrossrate:nargin");
    double level = 0.0;
    if (args.size() >= 2) level = args[1].toScalar();
    auto [r, c] = zerocrossrate(ctx.engine->resource(), args[0], level);
    outs[0] = r;
    if (nargout >= 2 && outs.size() >= 2) outs[1] = c;
}

void cusum_reg(Span<const Value> args, size_t nargout,
               Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("cusum: requires (x [, climit, mshift, tmean, tdev])",
                    0, 0, "cusum", "", "m:cusum:nargin");
    double climit = 5.0, mshift = 1.0, tmean = 0.0, tdev = 1.0;
    bool have_tmean = false, have_tdev = false;
    if (args.size() >= 2) climit = args[1].toScalar();
    if (args.size() >= 3) mshift = args[2].toScalar();
    if (args.size() >= 4) { tmean = args[3].toScalar(); have_tmean = true; }
    if (args.size() >= 5) { tdev  = args[4].toScalar(); have_tdev  = true; }
    auto R = cusum_impl(ctx.engine->resource(), args[0],
                        climit, mshift, have_tmean, tmean, have_tdev, tdev);
    outs[0] = R.iupper;
    if (nargout >= 2 && outs.size() >= 2) outs[1] = R.ilower;
    if (nargout >= 3 && outs.size() >= 3) outs[2] = R.uppersum;
    if (nargout >= 4 && outs.size() >= 4) outs[3] = R.lowersum;
}

} // namespace detail

} // namespace numkit::signal
