// libs/image/src/filter/filter_detail.hpp
//
// Private (src-only) declaration shared between the engine-free compute in
// filter.cpp and its CallContext register half in filter_reg.cpp. NOT part of
// the public image API — roifilt2_combine merges a filtered result back under
// a mask; both the filter form (compute roifilt2) and the function-handle form
// (roifilt2_reg) funnel through it.
//
// Phase 2b compute/register split — see project_layering_refactor memory.
#pragma once

#include <numkit/value/value.hpp>

#include <memory_resource>

namespace numkit::image {

// out = cast(I, class(filtRes)); out(BW) = filtRes(BW).
Value roifilt2_combine(const Value &I, const Value &filtRes, const Value &BW,
                       std::pmr::memory_resource *mr);

} // namespace numkit::image
