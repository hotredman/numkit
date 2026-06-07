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

#include <numkit/signal/measurements/sig_utils.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/value.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <utility>

namespace numkit::signal {

// ── seqperiod ─────────────────────────────────────────────────────────
std::pair<Value, Value>
seqperiod(const Value &x, double tol, std::pmr::memory_resource *mr)
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
zerocrossrate(const Value &x, double level, std::pmr::memory_resource *mr)
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
// Standard CUSUM detector (see measurements/sig_utils.hpp for the public
// API + CusumResult). Returns first indices where each one-sided cumulative
// sum exceeds the climit threshold (in standard-deviation units).

CusumResult cusum(const Value &x, double climit, double mshift,
                  const Value &tmean, const Value &tdev,
                  std::pmr::memory_resource *mr)
{
    const bool have_tmean   = !tmean.isEmpty();
    const bool have_tdev    = !tdev.isEmpty();
    const double tmean_user = have_tmean ? tmean.toScalar() : 0.0;
    const double tdev_user  = have_tdev  ? tdev.toScalar()  : 0.0;
    const size_t N = x.numel();
    CusumResult R;
    R.uppersum = Value::matrix(N, N == 0 ? 0 : 1, ValueType::DOUBLE, mr);
    R.lowersum = Value::matrix(N, N == 0 ? 0 : 1, ValueType::DOUBLE, mr);
    R.iupper = Value::matrix(0, 0, ValueType::DOUBLE, mr);
    R.ilower = Value::matrix(0, 0, ValueType::DOUBLE, mr);
    if (N == 0) return R;

    // Defaults: tmean = mean(x(1:25)), tdev = std(x(1:25)).
    const size_t baseN = std::min<size_t>(25, N);
    double tmeanEff = tmean_user;
    double tdevEff  = tdev_user;
    if (!have_tmean) {
        double sum = 0.0;
        for (size_t i = 0; i < baseN; ++i) sum += x.elemAsDouble(i);
        tmeanEff = sum / static_cast<double>(baseN);
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
        tdevEff = std::sqrt(std::max(0.0, var));
    }
    if (tdevEff <= 0.0) tdevEff = 1.0;  // guard

    double *us = R.uppersum.doubleDataMut();
    double *ls = R.lowersum.doubleDataMut();
    double up = 0.0, lo = 0.0;
    long long iup_first = -1, ilo_first = -1;
    const double half_shift = 0.5 * mshift;
    for (size_t i = 0; i < N; ++i) {
        const double z = (x.elemAsDouble(i) - tmeanEff) / tdevEff;
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

} // namespace numkit::signal
