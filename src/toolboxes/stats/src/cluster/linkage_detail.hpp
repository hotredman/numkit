// toolboxes/stats/src/cluster/linkage_detail.hpp — private compute/register shared
// surface: the internal cophenet_full worker (cophenet's [c,d] core; def in
// linkage.cpp, external). Phase 2b compute/register split.
#pragma once

#include <nk/value/value.hpp>

#include <memory_resource>
#include <tuple>

namespace nk::stats {

std::tuple<Value, Value> cophenet_full(const Value &Z, const Value &Y,
                                       std::pmr::memory_resource *mr);

} // namespace nk::stats
