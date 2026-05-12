// libs/stats/include/numkit/stats/distributions/ncx2.hpp
//
// Noncentral chi-squared distribution. Parameters: k > 0 (df),
// λ ≥ 0 (noncentrality).
//   pdf:  ½·exp(−(x+λ)/2)·(x/λ)^((k−2)/4)·I_{(k−2)/2}(√(λx)),  λ > 0
//   cdf:  Σ_{j≥0} Poisson(j; λ/2) · chi2cdf(x; k + 2j)
//   X = Σ_{j ~ Poisson(λ/2)} chi²(k + 2j)  (mixture representation)

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

Value ncx2pdf(const Value &x, double k, double lambda, std::pmr::memory_resource *mr = nullptr);
Value ncx2cdf(const Value &x, double k, double lambda, std::pmr::memory_resource *mr = nullptr);
Value ncx2inv(const Value &p, double k, double lambda, std::pmr::memory_resource *mr = nullptr);
Value ncx2rnd(double k, double lambda, size_t rows = 1, size_t cols = 1, std::pmr::memory_resource *mr = nullptr);
std::tuple<double, double> ncx2stat(double k, double lambda);

} // namespace numkit::stats
