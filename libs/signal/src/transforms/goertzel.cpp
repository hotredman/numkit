// libs/signal/src/transforms/goertzel.cpp
//
// Goertzel single-bin DFT. Split from MDspGaps.

#include <numkit/signal/transforms/goertzel.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include "../dsp_helpers.hpp"   // Complex typedef

#include <cmath>
#include <complex>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::signal {

// Goertzel computes a single DFT bin via a 2nd-order IIR. For each
// 1-based bin index k in `ind` (1 == DC, 2 == lowest non-DC, ..., N
// == highest), output[k] = sum_n x[n] * exp(-2πi (k-1) n / N).
//
// Two-coefficient form. Numerically stable for reasonable N; matches
// MATLAB's `goertzel(x, ind)` to FP roundoff.
Value goertzel(Allocator &alloc, const Value &x, const Value &ind)
{
    const size_t N = x.numel();
    const size_t M = ind.numel();
    auto r = Value::complexMatrix(ind.dims().rows(), ind.dims().cols(), &alloc);

    if (N == 0) return r;

    const double *xd = x.doubleData();
    for (size_t m = 0; m < M; ++m) {
        const double k1based = ind.doubleData()[m];
        const double k0based = k1based - 1.0;
        const double w = 2.0 * M_PI * k0based / static_cast<double>(N);
        const double cw = std::cos(w);
        const double sw = std::sin(w);
        const double coeff = 2.0 * cw;

        double s_prev = 0.0, s_prev2 = 0.0;
        for (size_t n = 0; n < N; ++n) {
            const double s = xd[n] + coeff * s_prev - s_prev2;
            s_prev2 = s_prev;
            s_prev  = s;
        }
        // Recursive output before phase correction:
        //   y = s_prev - exp(-jw) * s_prev2
        //     = (s_prev - cos(w)*s_prev2) + j*sin(w)*s_prev2
        // True DFT bin X[k] = y * exp(-jw*(N-1)).
        const double y_re = s_prev - cw * s_prev2;
        const double y_im = sw * s_prev2;
        const double pcw = std::cos(w * static_cast<double>(N - 1));
        const double psw = std::sin(w * static_cast<double>(N - 1));
        const double out_re = y_re * pcw + y_im * psw;
        const double out_im = y_im * pcw - y_re * psw;
        r.complexDataMut()[m] = Complex(out_re, out_im);
    }
    return r;
}

namespace detail {

void goertzel_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                  CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("goertzel: requires (x, ind)",
                     0, 0, "goertzel", "", "m:goertzel:nargin");
    outs[0] = goertzel(ctx.engine->allocator(), args[0], args[1]);
}

} // namespace detail

} // namespace numkit::signal
