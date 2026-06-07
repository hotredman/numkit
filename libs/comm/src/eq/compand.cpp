// libs/comm/src/eq/compand.cpp
//
// MATLAB compand: μ-law and A-law compressor / expander.
//
// Signature: y = compand(x, param, V, method)
//   x       — input signal (any shape, double)
//   param   — μ for μ-law, A for A-law
//   V       — peak magnitude (input clipped to [-V, V] in spirit)
//   method  — 'mu/compressor' | 'mu/expander' |
//             'A/compressor'  | 'A/expander'
//
// μ-law compress:  y = sign(x) · V · log(1 + μ|x|/V) / log(1 + μ)
// μ-law expand:    x = sign(y) · (V/μ) · (exp(|y|/V · log(1+μ)) − 1)
//
// A-law compress, |x|/V ≤ 1/A: y = sign(x) · A|x| / (1 + log A)
//                       else : y = sign(x) · V · (1 + log(A|x|/V)) / (1 + log A)
//
// A-law expand,  |y|/V ≤ 1/(1 + log A): x = sign(y) · |y|·(1+log A)/A
//                              else : x = sign(y) · V · exp((1+log A)·|y|/V − 1)/A

#include <numkit/comm/eq/compand.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <cmath>
#include <string>

namespace numkit::comm {

Value compand(const Value &x, double param, double V,
              const std::string &method,
              std::pmr::memory_resource *mr)
{
    if (param <= 0.0)
        throw Error("compand: param (μ or A) must be positive",
                    0, 0, "compand", "", "numkit:compand:param");
    if (V <= 0.0)
        throw Error("compand: V (peak magnitude) must be positive",
                    0, 0, "compand", "", "numkit:compand:V");

    const bool isMu = (method.size() >= 2
                       && (method[0] == 'm' || method[0] == 'M')
                       && (method[1] == 'u' || method[1] == 'U'));
    const bool isA  = (method.size() >= 1
                       && (method[0] == 'a' || method[0] == 'A'));
    if (!isMu && !isA)
        throw Error("compand: method must start with 'mu/' or 'A/'",
                    0, 0, "compand", "", "numkit:compand:method");
    const bool isCompress =
        method.find("compressor") != std::string::npos;
    const bool isExpand =
        method.find("expander") != std::string::npos;
    if (!isCompress && !isExpand)
        throw Error("compand: method must end with '/compressor' or "
                    "'/expander'",
                    0, 0, "compand", "", "numkit:compand:method");

    Value out = Value::matrix(x.dims().rows(), x.dims().cols(),
                              ValueType::DOUBLE, mr);
    double *o = out.doubleDataMut();
    const size_t N = x.numel();

    if (isMu) {
        const double logMu = std::log(1.0 + param);
        if (isCompress) {
            for (size_t i = 0; i < N; ++i) {
                const double xi   = x.elemAsDouble(i);
                const double absx = std::abs(xi);
                const double mag  = V * std::log(1.0 + param * absx / V)
                                      / logMu;
                o[i] = (xi >= 0.0) ? mag : -mag;
            }
        } else {  // expander
            for (size_t i = 0; i < N; ++i) {
                const double yi   = x.elemAsDouble(i);
                const double absy = std::abs(yi);
                const double mag  = (V / param)
                                  * (std::exp(absy / V * logMu) - 1.0);
                o[i] = (yi >= 0.0) ? mag : -mag;
            }
        }
    } else {  // A-law
        const double logA = std::log(param);
        const double oneLogA = 1.0 + logA;
        if (isCompress) {
            const double thr = 1.0 / param;  // |x|/V threshold
            for (size_t i = 0; i < N; ++i) {
                const double xi   = x.elemAsDouble(i);
                const double absx = std::abs(xi);
                const double ratio = absx / V;
                double mag;
                if (ratio <= thr) {
                    mag = (param * absx) / oneLogA;
                } else {
                    mag = V * (1.0 + std::log(param * ratio)) / oneLogA;
                }
                o[i] = (xi >= 0.0) ? mag : -mag;
            }
        } else {  // expander
            const double thr = 1.0 / oneLogA;  // |y|/V threshold
            for (size_t i = 0; i < N; ++i) {
                const double yi   = x.elemAsDouble(i);
                const double absy = std::abs(yi);
                const double ratio = absy / V;
                double mag;
                if (ratio <= thr) {
                    mag = absy * oneLogA / param;
                } else {
                    mag = V * std::exp(oneLogA * ratio - 1.0) / param;
                }
                o[i] = (yi >= 0.0) ? mag : -mag;
            }
        }
    }
    return out;
}

} // namespace numkit::comm
