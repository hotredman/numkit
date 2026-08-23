/// @file spec_driven.hpp
/// @ingroup group_signal
// toolboxes/signal/include/numkit/signal/digital_filtering/spec_driven.hpp
//
// Spec-driven filter wrappers: lowpass / highpass / bandpass / bandstop.
// One-call signal-in → signal-out, with sample rate provided. Internally
// use Butterworth design + zero-phase filtfilt (default order 8).

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

namespace numkit::signal {

/// @addtogroup group_signal
/// @{


/// One-call zero-phase Butterworth lowpass filter.
///
/// Designs an order-`order` Butterworth lowpass with cutoff `fpass` Hz,
/// then applies `filtfilt` for zero-phase response.
///
/// @param x      Input signal (any shape; columns filtered independently).
/// @param fpass  Cutoff frequency in Hz, `0 < fpass < fs/2`.
/// @param fs     Sample rate in Hz, > 0.
/// @param order  Filter order. Default 8.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Filtered signal, same shape as `x`.
///
/// @code  Value y = lowpass(audio, 4000.0, 48000.0);  // 4 kHz LP at 48 kHz  @endcode
///
/// @see highpass, bandpass, bandstop, butter
Value lowpass(const Value &                x,
              double                       fpass,
              double                       fs,
              int                          order = 8,
              std::pmr::memory_resource *  mr    = nullptr);

/// One-call zero-phase Butterworth highpass filter.
///
/// @param x      Input signal.
/// @param fpass  Cutoff frequency in Hz, `0 < fpass < fs/2`.
/// @param fs     Sample rate in Hz, > 0.
/// @param order  Filter order. Default 8.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Filtered signal.
///
/// @see lowpass
Value highpass(const Value &                x,
               double                       fpass,
               double                       fs,
               int                          order = 8,
               std::pmr::memory_resource *  mr    = nullptr);

/// One-call zero-phase Butterworth bandpass filter.
///
/// Implemented as a cascade of `highpass(x, flo)` then `lowpass(x, fhi)`.
/// For narrow bands consider designing a single bandpass filter via
/// `butter(N, [flo fhi]/fs*2, "bandpass")` and `filtfilt` directly.
///
/// @param x      Input signal.
/// @param flo    Lower cutoff in Hz.
/// @param fhi    Upper cutoff in Hz (`flo < fhi < fs/2`).
/// @param fs     Sample rate in Hz.
/// @param order  Order per cascade stage. Default 8.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Filtered signal.
///
/// @note Cascaded-stage approximation; accurate enough for typical wide
///       passbands. For sharp transitions design a single-stage BP
///       filter directly.
Value bandpass(const Value &                x,
               double                       flo,
               double                       fhi,
               double                       fs,
               int                          order = 8,
               std::pmr::memory_resource *  mr    = nullptr);

/// One-call zero-phase Butterworth bandstop (notch) filter.
///
/// Implemented as the additive inverse: `bandstop(x) = lowpass(x, flo) +
/// highpass(x, fhi)`. Cancellation in the stopband is approximate.
///
/// @param x      Input signal.
/// @param flo    Lower stopband edge in Hz.
/// @param fhi    Upper stopband edge in Hz (`flo < fhi < fs/2`).
/// @param fs     Sample rate in Hz.
/// @param order  Order per stage. Default 8.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Filtered signal.
Value bandstop(const Value &                x,
               double                       flo,
               double                       fhi,
               double                       fs,
               int                          order = 8,
               std::pmr::memory_resource *  mr    = nullptr);


/// @}
} // namespace numkit::signal
