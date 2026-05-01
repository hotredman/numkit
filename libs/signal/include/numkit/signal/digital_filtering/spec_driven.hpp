// libs/signal/include/numkit/signal/digital_filtering/spec_driven.hpp
//
// Spec-driven filter wrappers: lowpass / highpass / bandpass / bandstop.
// One-call signal-in → signal-out, with sample rate provided.
// Internally use Butterworth design + zero-phase filtfilt (default order 8).

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::signal {

/// lowpass(x, fpass, fs) — zero-phase Butterworth lowpass at order 8.
/// fpass is the cutoff in Hz, fs the sample rate. Returns the filtered
/// signal with the same shape as x.
Value lowpass(std::pmr::memory_resource *mr, const Value &x,
              double fpass, double fs, int order = 8);

/// highpass(x, fpass, fs) — same as lowpass but high-side.
Value highpass(std::pmr::memory_resource *mr, const Value &x,
               double fpass, double fs, int order = 8);

/// bandpass(x, [flo fhi], fs) — implemented as cascaded high+low pass
/// (filter twice through filtfilt). Edge-case approximation; accurate
/// enough for typical wide passbands. Default order 8 per stage.
Value bandpass(std::pmr::memory_resource *mr, const Value &x,
               double flo, double fhi, double fs, int order = 8);

/// bandstop(x, [flo fhi], fs) — cascaded low(flo) + high(fhi) on the
/// inverse path: x_out = lowpass(x, flo) + highpass(x, fhi).
/// Cancellation in the stopband is approximate.
Value bandstop(std::pmr::memory_resource *mr, const Value &x,
               double flo, double fhi, double fs, int order = 8);

} // namespace numkit::signal
