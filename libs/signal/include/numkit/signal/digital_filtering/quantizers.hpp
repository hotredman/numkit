// libs/signal/include/numkit/signal/digital_filtering/quantizers.hpp
//
// MATLAB Signal Toolbox uniform-quantization helpers (Phase 4.2):
//   uencode — float → integer (signed/unsigned, N bits, peak V)
//   udecode — integer → float (saturate/wrap on overflow)

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::signal {

// Quantize and encode the data in `u` to N-bit integers.
// V (default 1.0) sets the peak input value (saturates at ±V).
// `signedOutput=false` (default) → uint8/uint16/uint32 output;
// `signedOutput=true`  → int8/int16/int32 output.
// N must be in [2, 32]. Output type chosen by least-bits rule
// (≤8 → 8-bit, ≤16 → 16-bit, else 32-bit).
Value uencode(std::pmr::memory_resource *mr,
              const Value &u, int N, double V = 1.0,
              bool signedOutput = false);

// Decode integer data back to double, with peak value ±V (default 1.0).
// `wrapOnOverflow=false` (default) → saturate; `=true` → wrap modulo 2^N.
// Input must be one of int8/16/32 or uint8/16/32.
Value udecode(std::pmr::memory_resource *mr,
              const Value &u, int N, double V = 1.0,
              bool wrapOnOverflow = false);

} // namespace numkit::signal
