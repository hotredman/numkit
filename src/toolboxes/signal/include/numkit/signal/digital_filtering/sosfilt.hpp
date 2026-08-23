/// @file sosfilt.hpp
/// @ingroup group_signal
// toolboxes/signal/include/numkit/signal/digital_filtering/sosfilt.hpp
//
// Apply an SOS biquad cascade. Conversions zp2sos / tf2sos live in
// filter_implementation/conversions.hpp.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit::signal {

/// @addtogroup group_signal
/// @{


/// Apply a second-order-sections cascade to a signal.
///
/// Each row of `sos` represents one biquad section
/// `[b0 b1 b2 1 a1 a2]` (after global-gain normalisation; pass `g` to
/// `sos2tf` or build SOS via `zp2sos` for the gain-distributed form).
/// Sections are evaluated in cascade left-to-right.
///
/// For matrix `x`, columns are filtered independently.
///
/// @param sos  L × 6 SOS matrix.
/// @param x    Input signal (vector or 2-D matrix).
/// @param mr   Memory resource (nullptr → process default).
/// @return     Filtered signal, same shape as `x`.
///
/// @code
/// auto [z, p, k] = ellipap(8, 1.0, 60.0);
/// Value sos      = zp2sos(z, p, k);
/// Value y        = sosfilt(sos, x);
/// @endcode
///
/// @see sosfiltfilt, filter, zp2sos
Value sosfilt(const Value &                sos,
              const Value &                x,
              std::pmr::memory_resource *  mr = nullptr);

/// Zero-phase forward + backward SOS cascade.
///
/// Equivalent to applying `sosfilt` once forward and once on the
/// reversed signal (and reversing again), with Gustafsson initial
/// conditions and reflection padding to suppress edge transients.
/// This is the `filtfilt(d, x)` form where `d` is a digital-filter SOS
/// object.
///
/// Numerically stable for high-order IIR designs where direct (b, a)
/// form is ill-conditioned.
///
/// @param sos  L × 6 SOS matrix.
/// @param x    Input signal.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Zero-phase filtered signal.
///
/// @see sosfilt, filtfilt
Value sosfiltfilt(const Value &                sos,
                  const Value &                x,
                  std::pmr::memory_resource *  mr = nullptr);


/// @}
} // namespace numkit::signal
