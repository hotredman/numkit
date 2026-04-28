// libs/signal/src/transforms/dct.cpp
//
// Type-II Discrete Cosine Transform + inverse (MATLAB default).
// Split from libs/signal/src/. Direct O(N²); FFT-based path can be added
// later if benches show dct hot.

#include <numkit/signal/transforms/dct.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::signal {

// dct:  X[k] = w[k] * sum_n x[n] * cos(pi (2n+1) k / (2N))
// idct: x[n] = sum_k w[k] * X[k] * cos(pi (2n+1) k / (2N))
//   where w[0] = sqrt(1/N), w[k>0] = sqrt(2/N).
Value dct(Allocator &alloc, const Value &x)
{
    const size_t N = x.numel();
    auto r = createLike(x, ValueType::DOUBLE, &alloc);
    if (N == 0) return r;
    const double *xd = x.doubleData();
    double *X  = r.doubleDataMut();

    const double w0 = std::sqrt(1.0 / static_cast<double>(N));
    const double wk = std::sqrt(2.0 / static_cast<double>(N));
    const double piOver2N = M_PI / (2.0 * static_cast<double>(N));

    for (size_t k = 0; k < N; ++k) {
        double acc = 0.0;
        const double angK = piOver2N * static_cast<double>(k);
        for (size_t n = 0; n < N; ++n)
            acc += xd[n] * std::cos(angK * static_cast<double>(2 * n + 1));
        X[k] = (k == 0 ? w0 : wk) * acc;
    }
    return r;
}

Value idct(Allocator &alloc, const Value &x)
{
    const size_t N = x.numel();
    auto r = createLike(x, ValueType::DOUBLE, &alloc);
    if (N == 0) return r;
    const double *Xd = x.doubleData();
    double *xt = r.doubleDataMut();

    const double w0 = std::sqrt(1.0 / static_cast<double>(N));
    const double wk = std::sqrt(2.0 / static_cast<double>(N));
    const double piOver2N = M_PI / (2.0 * static_cast<double>(N));

    for (size_t n = 0; n < N; ++n) {
        double acc = w0 * Xd[0];  // k=0 term separated for the w0 weight
        const double angN = piOver2N * static_cast<double>(2 * n + 1);
        for (size_t k = 1; k < N; ++k)
            acc += wk * Xd[k] * std::cos(angN * static_cast<double>(k));
        xt[n] = acc;
    }
    return r;
}

namespace detail {

void dct_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
             CallContext &ctx)
{
    if (args.empty())
        throw Error("dct: requires 1 argument",
                     0, 0, "dct", "", "m:dct:nargin");
    outs[0] = dct(ctx.engine->allocator(), args[0]);
}

void idct_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
              CallContext &ctx)
{
    if (args.empty())
        throw Error("idct: requires 1 argument",
                     0, 0, "idct", "", "m:idct:nargin");
    outs[0] = idct(ctx.engine->allocator(), args[0]);
}

} // namespace detail

} // namespace numkit::signal
