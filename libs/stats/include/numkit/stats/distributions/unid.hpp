// libs/stats/include/numkit/stats/distributions/unid.hpp
//
// Discrete uniform on {1, 2, ..., N}.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

Value unidpdf(std::pmr::memory_resource *mr, const Value &k, double N);
Value unidcdf(std::pmr::memory_resource *mr, const Value &k, double N);
Value unidinv(std::pmr::memory_resource *mr, const Value &p, double N);
Value unidrnd(std::pmr::memory_resource *mr, double N,
              size_t rows = 1, size_t cols = 1);
std::tuple<double, double> unidstat(double N);

} // namespace numkit::stats
