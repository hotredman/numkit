// libs/comm/src/eq/pulse.cpp
//
// rcosdesign — raised-cosine and root-raised-cosine FIR design.
// Closed-form coefficients:
//   RC:   p(t) = sinc(t/T) · cos(πβt/T) / (1 − (2βt/T)²)
//   RRC:  h(t) = (4β / π√T) · [cos((1+β)πt/T)
//                  + sin((1−β)πt/T) / (4βt/T)]
//                / (1 − (4βt/T)²)
// with the standard l'Hôpital limits at t = 0 and t = ±T/(4β).
// Output normalised to unit energy, matching MATLAB R2025b.

#include <numkit/comm/eq/pulse.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::comm {

namespace {

double sinc_pi(double x) {
    if (std::abs(x) < 1e-12) return 1.0;
    const double a = M_PI * x;
    return std::sin(a) / a;
}

} // anonymous

Value rcosdesign(std::pmr::memory_resource *mr,
                 double beta, int span, int sps,
                 const std::string &shape)
{
    if (!(beta >= 0.0 && beta <= 1.0))
        throw Error("rcosdesign: beta must be in [0, 1]",
                    0, 0, "rcosdesign", "", "m:rcosdesign:beta");
    if (span <= 0)
        throw Error("rcosdesign: span must be a positive integer",
                    0, 0, "rcosdesign", "", "m:rcosdesign:span");
    if (sps <= 0)
        throw Error("rcosdesign: sps must be a positive integer",
                    0, 0, "rcosdesign", "", "m:rcosdesign:sps");

    const bool rrc = (shape == "sqrt" || shape == "Sqrt" || shape == "SQRT");
    if (!rrc && shape != "normal" && shape != "Normal" &&
        shape != "NORMAL" && !shape.empty())
        throw Error("rcosdesign: shape must be 'normal' or 'sqrt'",
                    0, 0, "rcosdesign", "", "m:rcosdesign:shape");

    // Filter length.
    const int N = span * sps + 1;
    std::vector<double> h(static_cast<size_t>(N), 0.0);

    // Symbol period T set so that t / T is dimensionless and t runs
    // from -span/2 to +span/2 in steps of 1/sps. (T = 1 here.)
    const double T = 1.0;
    const double dt = 1.0 / static_cast<double>(sps);

    for (int n = 0; n < N; ++n) {
        const double t = (n - (N - 1) / 2.0) * dt;
        double v;

        if (!rrc) {
            // Raised cosine.
            const double tT = t / T;
            const double denom = 1.0 - std::pow(2.0 * beta * tT, 2);
            if (std::abs(denom) < 1e-12) {
                // l'Hôpital limit at t = ±T/(2β).
                v = (M_PI / 4.0) * sinc_pi(1.0 / (2.0 * beta));
            } else {
                v = sinc_pi(tT) * std::cos(M_PI * beta * tT) / denom;
            }
        } else {
            // Root raised cosine.
            const double tT = t / T;
            if (std::abs(t) < 1e-12) {
                // h(0) = (1 + β · (4/π − 1)) / √T
                v = (1.0 + beta * (4.0 / M_PI - 1.0)) / std::sqrt(T);
            } else if (beta > 0.0 &&
                       std::abs(std::abs(t) - T / (4.0 * beta)) < 1e-12) {
                // h(±T/(4β)) limit
                const double a = (beta / std::sqrt(2.0 * T));
                const double s = std::sin(M_PI / (4.0 * beta));
                const double c = std::cos(M_PI / (4.0 * beta));
                v = a * ((1.0 + 2.0 / M_PI) * s + (1.0 - 2.0 / M_PI) * c);
            } else {
                const double num = std::cos((1.0 + beta) * M_PI * tT) +
                                    std::sin((1.0 - beta) * M_PI * tT) /
                                        (4.0 * beta * tT);
                const double den = 1.0 - std::pow(4.0 * beta * tT, 2);
                v = (4.0 * beta / (M_PI * std::sqrt(T))) * num / den;
            }
        }
        h[static_cast<size_t>(n)] = v;
    }

    // Unit-energy normalisation (matches MATLAB).
    double e = 0.0;
    for (double v : h) e += v * v;
    if (e > 0.0) {
        const double s = 1.0 / std::sqrt(e);
        for (auto &v : h) v *= s;
    }

    Value r = Value::matrix(1, static_cast<size_t>(N), ValueType::DOUBLE, mr);
    if (N > 0) std::copy(h.begin(), h.end(), r.doubleDataMut());
    return r;
}

// ── gaussdesign ─────────────────────────────────────────────────────
// Per MATLAB R2025b's gaussdesign.m:
//   filtLen = span*sps + 1
//   t       = ((1:filtLen) - mean(1:filtLen)) / sps
//   alpha   = sqrt(log(2)/2) / BT
//   h       = (sqrt(pi)/alpha) * exp(-(pi*t/alpha).^2)
//   h       = h / sum(h)                                (unit-area)
Value gaussdesign(std::pmr::memory_resource *mr,
                  double BT, int span, int sps)
{
    if (!(BT > 0.0))
        throw Error("gaussdesign: BT must be positive",
                    0, 0, "gaussdesign", "", "m:gaussdesign:BT");
    if (span <= 0)
        throw Error("gaussdesign: span must be a positive integer",
                    0, 0, "gaussdesign", "", "m:gaussdesign:span");
    if (sps <= 0)
        throw Error("gaussdesign: sps must be a positive integer",
                    0, 0, "gaussdesign", "", "m:gaussdesign:sps");

    const int N = span * sps + 1;
    const double centre = 0.5 * (1 + N);  // mean(1:N)
    const double alpha = std::sqrt(std::log(2.0) / 2.0) / BT;
    const double pre = std::sqrt(M_PI) / alpha;

    std::vector<double> h(static_cast<size_t>(N));
    double s = 0.0;
    for (int k = 1; k <= N; ++k) {
        const double t = (static_cast<double>(k) - centre) / sps;
        const double arg = M_PI * t / alpha;
        const double v = pre * std::exp(-(arg * arg));
        h[static_cast<size_t>(k - 1)] = v;
        s += v;
    }
    if (s > 0.0) {
        const double inv = 1.0 / s;
        for (auto &v : h) v *= inv;
    }

    Value r = Value::matrix(1, static_cast<size_t>(N), ValueType::DOUBLE, mr);
    if (N > 0) std::copy(h.begin(), h.end(), r.doubleDataMut());
    return r;
}

// ── rectpulse ───────────────────────────────────────────────────────
// rectpulse(x, n) repeats each sample of x n times along the leading
// non-singleton dimension. Vector inputs preserve orientation; matrix
// inputs repeat each row n times (column count unchanged).
Value rectpulse(std::pmr::memory_resource *mr, const Value &x, int n)
{
    if (n <= 0)
        throw Error("rectpulse: n must be a positive integer",
                    0, 0, "rectpulse", "", "m:rectpulse:n");

    const auto &d = x.dims();
    const size_t H = d.rows();
    const size_t W = d.cols();
    const bool is_row = (H == 1 && W >= 1);
    const bool is_col = (W == 1 && H >= 1);

    Value out;
    if (is_row) {
        out = Value::matrix(1, W * static_cast<size_t>(n), ValueType::DOUBLE, mr);
        double *od = out.doubleDataMut();
        for (size_t c = 0; c < W; ++c) {
            const double v = x.elemAsDouble(c);
            for (int k = 0; k < n; ++k)
                od[c * static_cast<size_t>(n) + static_cast<size_t>(k)] = v;
        }
    } else if (is_col) {
        out = Value::matrix(H * static_cast<size_t>(n), 1, ValueType::DOUBLE, mr);
        double *od = out.doubleDataMut();
        for (size_t r = 0; r < H; ++r) {
            const double v = x.elemAsDouble(r);
            for (int k = 0; k < n; ++k)
                od[r * static_cast<size_t>(n) + static_cast<size_t>(k)] = v;
        }
    } else {
        const size_t Nh = H * static_cast<size_t>(n);
        out = Value::matrix(Nh, W, ValueType::DOUBLE, mr);
        double *od = out.doubleDataMut();
        for (size_t c = 0; c < W; ++c) {
            for (size_t r = 0; r < H; ++r) {
                const double v = x.elemAsDouble(c * H + r);
                for (int k = 0; k < n; ++k)
                    od[c * Nh + r * static_cast<size_t>(n) + static_cast<size_t>(k)] = v;
            }
        }
    }
    return out;
}

// ── intdump ─────────────────────────────────────────────────────────
// Inverse of rectpulse: average each n consecutive samples along the
// leading non-singleton dimension. Length along that axis must be
// divisible by n.
Value intdump(std::pmr::memory_resource *mr, const Value &x, int n)
{
    if (n <= 0)
        throw Error("intdump: n must be a positive integer",
                    0, 0, "intdump", "", "m:intdump:n");

    const auto &d = x.dims();
    const size_t H = d.rows();
    const size_t W = d.cols();
    const bool is_row = (H == 1 && W >= 1);
    const bool is_col = (W == 1 && H >= 1);
    const size_t un = static_cast<size_t>(n);

    Value out;
    if (is_row) {
        if (W % un != 0)
            throw Error("intdump: row length must be a multiple of n",
                        0, 0, "intdump", "", "m:intdump:size");
        const size_t Wo = W / un;
        out = Value::matrix(1, Wo, ValueType::DOUBLE, mr);
        double *od = out.doubleDataMut();
        for (size_t c = 0; c < Wo; ++c) {
            double s = 0.0;
            for (size_t k = 0; k < un; ++k)
                s += x.elemAsDouble(c * un + k);
            od[c] = s / static_cast<double>(un);
        }
    } else if (is_col) {
        if (H % un != 0)
            throw Error("intdump: column length must be a multiple of n",
                        0, 0, "intdump", "", "m:intdump:size");
        const size_t Ho = H / un;
        out = Value::matrix(Ho, 1, ValueType::DOUBLE, mr);
        double *od = out.doubleDataMut();
        for (size_t r = 0; r < Ho; ++r) {
            double s = 0.0;
            for (size_t k = 0; k < un; ++k)
                s += x.elemAsDouble(r * un + k);
            od[r] = s / static_cast<double>(un);
        }
    } else {
        // Matrix: average n consecutive rows per column.
        if (H % un != 0)
            throw Error("intdump: row count must be a multiple of n",
                        0, 0, "intdump", "", "m:intdump:size");
        const size_t Ho = H / un;
        out = Value::matrix(Ho, W, ValueType::DOUBLE, mr);
        double *od = out.doubleDataMut();
        for (size_t c = 0; c < W; ++c) {
            for (size_t r = 0; r < Ho; ++r) {
                double s = 0.0;
                for (size_t k = 0; k < un; ++k)
                    s += x.elemAsDouble(c * H + r * un + k);
                od[c * Ho + r] = s / static_cast<double>(un);
            }
        }
    }
    return out;
}

namespace detail {

void rcosdesign_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("rcosdesign: requires (beta, span, sps [, shape])",
                    0, 0, "rcosdesign", "", "m:rcosdesign:nargin");
    const double beta = args[0].toScalar();
    const int span    = static_cast<int>(args[1].toScalar());
    const int sps     = static_cast<int>(args[2].toScalar());
    std::string shape = "normal";
    if (args.size() >= 4 && !args[3].isEmpty()) {
        if (!args[3].isChar() && !args[3].isString())
            throw Error("rcosdesign: shape must be a string",
                        0, 0, "rcosdesign", "", "m:rcosdesign:shape");
        shape = args[3].toString();
    }
    outs[0] = rcosdesign(ctx.engine->resource(), beta, span, sps, shape);
}

void gaussdesign_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("gaussdesign: requires (BT, span, sps)",
                    0, 0, "gaussdesign", "", "m:gaussdesign:nargin");
    const double BT  = args[0].toScalar();
    const int span   = static_cast<int>(args[1].toScalar());
    const int sps    = static_cast<int>(args[2].toScalar());
    outs[0] = gaussdesign(ctx.engine->resource(), BT, span, sps);
}

void rectpulse_reg(Span<const Value> args, size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("rectpulse: requires (x, n)",
                    0, 0, "rectpulse", "", "m:rectpulse:nargin");
    const int n = static_cast<int>(args[1].toScalar());
    outs[0] = rectpulse(ctx.engine->resource(), args[0], n);
}

void intdump_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("intdump: requires (x, n)",
                    0, 0, "intdump", "", "m:intdump:nargin");
    const int n = static_cast<int>(args[1].toScalar());
    outs[0] = intdump(ctx.engine->resource(), args[0], n);
}

} // namespace detail

} // namespace numkit::comm
