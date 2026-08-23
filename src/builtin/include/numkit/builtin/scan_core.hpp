// src/builtin/include/numkit/builtin/scan_core.hpp
#pragma once

// Shared scan parse+shape core, exposed so the engine-coupled fscanf shim
// (relocated to the runtime layer) can reuse the same machinery as
// the pure sscanf. scanfEmit itself is pure — no Engine.

#include <numkit/value/span.hpp>
#include <numkit/value/value.hpp>

#include <cstddef>
#include <memory_resource>
#include <string>

namespace numkit::ops {
struct SizeSpec;
}

namespace numkit::builtin::detail {

// scanfCycle's run summary — matched-item count + input bytes consumed.
struct ScanfOut
{
    std::size_t count;
    std::size_t bytesConsumed;
};

// Common fscanf/sscanf body once the input buffer has been materialised:
// parse `input` per `fmt` (cyclically, bounded by `sz`), write the shaped
// result into outs[0] and (when nargout > 1) the count into outs[1].
void scanfEmit(const std::string &input, const std::string &fmt,
               const ::numkit::ops::SizeSpec &sz, std::size_t nargout,
               Span<Value> outs, std::pmr::memory_resource *mr, ScanfOut &r);

} // namespace numkit::builtin::detail
