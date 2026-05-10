// libs/signal/include/numkit/signal/digital_filtering/poly_utils.hpp
//
// MATLAB Signal Toolbox polynomial utilities (Phase 4.3):
//   polyscale — radial scaling of polynomial roots in z-plane
//   polystab  — reflect roots outside unit circle to inside

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::signal {

// polyscale(p, scale) — y[k] = p[k] * scale^k for k = 0..N-1.
// Equivalent to scaling the roots of polynomial p in the z-plane.
// Input may be real or complex; scale may be real or complex.
Value polyscale(std::pmr::memory_resource *mr,
                const Value &p, const Value &scale);

// polystab(a) — reflect any root with |root|>1 to its reciprocal-conjugate
// inside the unit circle, preserving magnitude response. Returns the
// resulting polynomial coefficient vector (real if input was real).
Value polystab(std::pmr::memory_resource *mr, const Value &a);

} // namespace numkit::signal
