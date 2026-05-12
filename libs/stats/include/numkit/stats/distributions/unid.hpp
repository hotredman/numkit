// libs/stats/include/numkit/stats/distributions/unid.hpp
//
// Discrete uniform on {1, 2, ..., N}.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::stats {

Value unidpdf(const Value &k, double N, std::pmr::memory_resource *mr = nullptr);
Value unidcdf(const Value &k, double N, std::pmr::memory_resource *mr = nullptr);
Value unidinv(const Value &p, double N, std::pmr::memory_resource *mr = nullptr);
Value unidrnd(double N, size_t rows = 1, size_t cols = 1, std::pmr::memory_resource *mr = nullptr);
std::tuple<double, double> unidstat(double N);

} // namespace numkit::stats
