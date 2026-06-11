// toolboxes/signal/src/transforms/dct_detail.hpp
//
// Private (src-only) helper shared between the engine-free compute in dct.cpp
// and its CallContext register half in dct_reg.cpp. NOT part of the public
// signal API — `dctTyped` is the internal arbitrary-Type (1..4) / length /
// dim dispatcher that backs the dct()/idct() 'Type' name-value branch; the
// public surface is only dct()/idct().
//
// Phase 2b compute/register split — see project_layering_refactor memory.
#pragma once

#include <numkit/value/value.hpp>

#include <memory_resource>

namespace numkit::signal {

// Length-override / dim wrapper for an arbitrary DCT type (1..4), forward or
// inverse. type==2 forward / type==3 inverse coincide with dct()/idct().
Value dctTyped(const Value &x, int n, int dim, double type, bool inverse,
               std::pmr::memory_resource *mr);

} // namespace numkit::signal
