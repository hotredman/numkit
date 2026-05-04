// libs/wavelet/src/swt/swt.cpp
//
// Stationary (à trous) wavelet transform: same length as the input at
// every level, no decimation. The forward analyses with dilated
// filters at each level (zeros inserted between taps); the inverse is
// the *transpose* of the forward operator, divided by 2 to cancel the
// per-level redundancy.
//
// Forward (correlation, periodic boundary, dilation 2^k at level k+1):
//   a_{k+1}[n] = sum_l Lo_D[l] · a_k[(n + l·2^k) mod N]
//   d_{k+1}[n] = sum_l Hi_D[l] · a_k[(n + l·2^k) mod N]
//
// In matrix form a = H·a_prev, d = G·a_prev with H, G circulant
// shifted-stride filters. SWT preserves dim per band per level, so
// H·Hᵀ + G·Gᵀ = 2·I. Hence the exact inverse is
//   a_prev = (1/2)·(Hᵀ·a + Gᵀ·d)
// and the transpose of a "look-forward" correlation is a "look-back"
// correlation with the same coefficients:
//   a_prev[n] = (1/2)·{ sum_l Lo_D[l] · a[(n − l·2^k) mod N]
//                     + sum_l Hi_D[l] · d[(n − l·2^k) mod N] }
//
// MATLAB layout returned/consumed:
//   row 1..n : details   (level 1 = finest, level n = coarsest)
//   row n+1  : approximation at level n

#include <numkit/wavelet/swt/swt.hpp>
#include <numkit/wavelet/filter/wfilters.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace numkit::wavelet {

namespace {

// y[i] = sum_j h[j] · x[(i + j·step) mod N]  (forward / analysis)
std::vector<double> circ_corr_fwd(const std::vector<double> &x,
                                  const std::vector<double> &h,
                                  size_t step)
{
    const size_t N = x.size();
    std::vector<double> y(N, 0.0);
    if (N == 0 || h.empty()) return y;
    for (size_t i = 0; i < N; ++i) {
        double acc = 0.0;
        for (size_t j = 0; j < h.size(); ++j) {
            const size_t idx = (i + j * step) % N;
            acc += h[j] * x[idx];
        }
        y[i] = acc;
    }
    return y;
}

// y[i] = sum_j h[j] · x[(i − j·step) mod N]  (transpose of fwd)
std::vector<double> circ_corr_back(const std::vector<double> &x,
                                   const std::vector<double> &h,
                                   size_t step)
{
    const size_t N = x.size();
    std::vector<double> y(N, 0.0);
    if (N == 0 || h.empty()) return y;
    for (size_t i = 0; i < N; ++i) {
        double acc = 0.0;
        for (size_t j = 0; j < h.size(); ++j) {
            // Modular subtract: ((i − j·step) mod N) ≡ ((i + N − (j·step mod N)) mod N).
            const size_t shift = (j * step) % N;
            const size_t idx = (i + N - shift) % N;
            acc += h[j] * x[idx];
        }
        y[i] = acc;
    }
    return y;
}

std::vector<double> rowOf(const Value &M, size_t row, size_t cols) {
    std::vector<double> r(cols);
    const size_t H = M.dims().rows();
    for (size_t c = 0; c < cols; ++c)
        r[c] = M.elemAsDouble(c * H + row);
    return r;
}

} // anonymous

Value swt(std::pmr::memory_resource *mr,
          const Value &x, int n, const std::string &wname)
{
    if (n < 1)
        throw Error("swt: level must be ≥ 1",
                    0, 0, "swt", "", "m:swt:level");
    const size_t N = x.numel();
    if (N == 0)
        return Value::matrix(static_cast<size_t>(n + 1), 0,
                             ValueType::DOUBLE, mr);

    const size_t needed = static_cast<size_t>(1) << n;
    if (N % needed != 0)
        throw Error("swt: signal length must be divisible by 2^n",
                    0, 0, "swt", "", "m:swt:size");

    auto fb = wavelet_filters(wname);
    std::vector<double> a(N);
    for (size_t i = 0; i < N; ++i) a[i] = x.elemAsDouble(i);

    Value out = Value::matrix(static_cast<size_t>(n + 1), N,
                              ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    const size_t H = static_cast<size_t>(n + 1);

    for (int k = 0; k < n; ++k) {
        const size_t step = static_cast<size_t>(1) << k; // 2^k
        auto a_next = circ_corr_fwd(a, fb.Lo_D, step);
        auto d_next = circ_corr_fwd(a, fb.Hi_D, step);
        // Detail row k (level k+1, 1-based) → MATLAB row index k.
        for (size_t c = 0; c < N; ++c) od[c * H + k] = d_next[c];
        a = std::move(a_next);
    }
    for (size_t c = 0; c < N; ++c) od[c * H + n] = a[c];
    return out;
}

Value iswt(std::pmr::memory_resource *mr,
           const Value &swc, const std::string &wname)
{
    const size_t H = swc.dims().rows();
    const size_t N = swc.dims().cols();
    if (H < 2)
        throw Error("iswt: input must have at least 2 rows",
                    0, 0, "iswt", "", "m:iswt:size");
    const int n = static_cast<int>(H) - 1;
    if (N == 0)
        return Value::matrix(1, 0, ValueType::DOUBLE, mr);

    auto fb = wavelet_filters(wname);
    auto a = rowOf(swc, static_cast<size_t>(n), N);

    // Walk from coarsest (k = n-1) to finest (k = 0). At each level
    // apply the transpose of the forward correlation and divide by 2.
    for (int k = n - 1; k >= 0; --k) {
        const size_t step = static_cast<size_t>(1) << k;
        auto d = rowOf(swc, static_cast<size_t>(k), N);
        auto lo = circ_corr_back(a, fb.Lo_D, step);
        auto hi = circ_corr_back(d, fb.Hi_D, step);
        for (size_t i = 0; i < N; ++i) a[i] = 0.5 * (lo[i] + hi[i]);
    }

    Value y = Value::matrix(1, N, ValueType::DOUBLE, mr);
    if (N > 0) std::copy(a.begin(), a.end(), y.doubleDataMut());
    return y;
}

namespace detail {

static std::string argString(const Value &v) {
    if (!v.isChar() && !v.isString())
        throw Error("wavelet: expected string argument",
                    0, 0, "", "", "m:wavelet:type");
    return v.toString();
}

void swt_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
             CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("swt: requires (x, n, wname)",
                    0, 0, "swt", "", "m:swt:nargin");
    outs[0] = swt(ctx.engine->resource(),
                  args[0], static_cast<int>(args[1].toScalar()),
                  argString(args[2]));
}

void iswt_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
              CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("iswt: requires (swc, wname)",
                    0, 0, "iswt", "", "m:iswt:nargin");
    outs[0] = iswt(ctx.engine->resource(), args[0], argString(args[1]));
}

} // namespace detail

} // namespace numkit::wavelet
