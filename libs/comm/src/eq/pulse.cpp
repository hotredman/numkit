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

} // namespace detail

} // namespace numkit::comm
