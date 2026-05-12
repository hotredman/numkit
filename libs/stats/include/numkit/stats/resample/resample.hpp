// libs/stats/include/numkit/stats/resample/resample.hpp

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::stats {

/// randsample(N, K[, replacement, weights]) — sample K integers from
/// 1..N. When `replacement = false` (default), draw without replacement.
Value randsample(int N, int K, bool with_replacement, const Value &weights, std::pmr::memory_resource *mr = nullptr);

/// datasample(X, K[, dim, replacement, weights]) — sample K rows
/// (default) or columns of X.
Value datasample(const Value &X, int K, int dim, bool with_replacement, const Value &weights, std::pmr::memory_resource *mr = nullptr);

/// bootstrp(nboot, fn, X) — call fn(resample_of_X) nboot times,
/// each time with rows drawn with replacement. Returns nboot×D where
/// D matches the output of fn applied to one bootstrap sample.
Value bootstrp(int nboot, const Value &fn, const Value &X, std::pmr::memory_resource *mr = nullptr);

/// jackknife(fn, X) — leave-one-out estimates. Returns N×D.
Value jackknife(const Value &fn, const Value &X, std::pmr::memory_resource *mr = nullptr);

/// combnk(N, K) — enumerate all C(N, K) k-combinations, returning a
/// C(N, K) × K matrix. The N argument can be a scalar (treated as
/// 1..N) or a vector (combinations of its elements).
Value combnk(const Value &v, int K, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::stats
