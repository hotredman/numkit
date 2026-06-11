// toolboxes/stats/src/dim/pca_detail.hpp — private compute/register shared surface:
// pcares_full (residuals+reconstruction core; def in pca.cpp, external).
#pragma once
#include <numkit/value/value.hpp>
#include <memory_resource>
#include <tuple>
namespace numkit::stats {
std::tuple<Value, Value> pcares_full(const Value &X, int ndim,
                                     std::pmr::memory_resource *mr);
} // namespace numkit::stats
