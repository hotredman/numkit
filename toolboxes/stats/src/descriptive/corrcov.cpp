// toolboxes/stats/src/descriptive/corrcov.cpp
//
// MATLAB corrcov(C) — correlation matrix derived from a covariance C.
//
//   R = corrcov(C)
//   [R, sigma] = corrcov(C)
//
// R(i, j) = C(i, j) / sqrt(C(i, i) * C(j, j))
// sigma(i) = sqrt(C(i, i))     (standard deviations, returned as a row)
//
// C must be square. Entries on the diagonal must be non-negative
// (variances); a negative diagonal entry is rejected.
// Off-diagonal divisions where the denominator is zero produce
// NaN, matching MATLAB R2025b.

#include <numkit/stats/descriptive/descriptive.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <cmath>
#include <limits>
#include <vector>

namespace numkit::stats {

std::pair<Value, Value>
corrcov(const Value &C, std::pmr::memory_resource *mr)
{
    const size_t R = C.dims().rows();
    const size_t Cn = C.dims().cols();
    if (R != Cn)
        throw Error("corrcov: input must be a square matrix",
                    0, 0, "corrcov", "", "numkit:corrcov:NotSquare");
    if (R == 0)
        throw Error("corrcov: input must be non-empty",
                    0, 0, "corrcov", "", "numkit:corrcov:Empty");

    // sigma(i) = sqrt(C(i, i)).
    std::vector<double> sigma(R);
    for (size_t i = 0; i < R; ++i) {
        const double v = C.elemAsDouble(i + i * R);   // column-major
        if (v < 0.0)
            throw Error("corrcov: negative variance on diagonal",
                        0, 0, "corrcov", "",
                        "numkit:corrcov:NegativeVar");
        sigma[i] = std::sqrt(v);
    }

    Value Rv = Value::matrix(R, R, ValueType::DOUBLE, mr);
    double *o = Rv.doubleDataMut();
    for (size_t j = 0; j < R; ++j) {
        for (size_t i = 0; i < R; ++i) {
            const double cij = C.elemAsDouble(i + j * R);
            const double denom = sigma[i] * sigma[j];
            o[i + j * R] = (denom == 0.0)
                               ? std::numeric_limits<double>::quiet_NaN()
                               : cij / denom;
        }
    }
    // Sigma as row vector (MATLAB convention).
    Value Sv = Value::matrix(1, R, ValueType::DOUBLE, mr);
    double *s = Sv.doubleDataMut();
    for (size_t i = 0; i < R; ++i) s[i] = sigma[i];

    return {std::move(Rv), std::move(Sv)};
}

} // namespace numkit::stats
