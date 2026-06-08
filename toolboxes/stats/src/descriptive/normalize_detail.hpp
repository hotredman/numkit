// toolboxes/stats/src/descriptive/normalize_detail.hpp — private compute/register
// shared surface: zscoreCore (the flag/dim zscore core with optional mu/sigma
// out-params; def in normalize.cpp, external). Phase 2b compute/register split.
#pragma once

#include <numkit/value/value.hpp>

#include <memory_resource>

namespace numkit::stats {

Value zscoreCore(const Value &A, int flag, int dim,
                 std::pmr::memory_resource *mr,
                 Value *muOut = nullptr, Value *sigmaOut = nullptr);

} // namespace numkit::stats
