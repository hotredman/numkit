// libs/signal/src/transforms/goertzel.cpp
//
// Goertzel single-bin DFT. Split from libs/signal/src/.

#include <numkit/signal/transforms/goertzel.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <numkit/core/scratch.hpp>

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
Value goertzel(const Value &x, const Value &ind, std::pmr::memory_resource *mr)
{
    const size_t N = x.numel();
    const size_t M = ind.numel();
    auto r = Value::complexMatrix(ind.dims().rows(), ind.dims().cols(), mr);

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
    if (args.empty())
        throw Error("goertzel: requires (x[, ind])",
                     0, 0, "goertzel", "", "numkit:goertzel:nargin");
    auto *mr = ctx.engine->resource();
    if (args.size() == 1 || args[1].isEmpty()) {
        // 1-arg form (or empty 2nd arg): MATLAB defaults `ind = 1:N`
        // and the output has the SAME SHAPE as x (per `doc goertzel`).
        // For a column input we need a column-shaped ind; for a row
        // input we need a row-shaped ind. The Goertzel kernel uses
        // ind.dims() to size the output, so building ind with the same
        // dims as x propagates the shape correctly.
        const Value &x = args[0];
        const size_t N = x.numel();
        const size_t rows = x.dims().rows();
        const size_t cols = x.dims().cols();
        Value ind = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
        double *id = ind.doubleDataMut();
        for (size_t i = 0; i < N; ++i)
            id[i] = static_cast<double>(i + 1);
        outs[0] = goertzel(x, ind, mr);
        return;
    }
    outs[0] = goertzel(args[0], args[1], mr);
}

} // namespace detail

} // namespace numkit::signal
