// libs/control/include/numkit/control/lti/lti.hpp
//
// LTI (Linear Time-Invariant) system constructors. Each returns a
// numkit struct value with a `kind` field tag ('tf', 'zpk', or 'ss')
// plus the canonical fields. Optional `Ts` field for discrete systems
// (>0); 0 means continuous-time.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::control {

/// `tf(num, den [, Ts])` — transfer function in the variable s
/// (or z if Ts > 0). num/den are row coefficient vectors with the
/// leading coefficient first (MATLAB convention).
Value tf(std::pmr::memory_resource *mr,
         const Value &num, const Value &den, double Ts);

/// `zpk(z, p, k [, Ts])` — zero-pole-gain form.
Value zpk(std::pmr::memory_resource *mr,
          const Value &z, const Value &p, const Value &k, double Ts);

/// `ss(A, B, C, D [, Ts])` — state-space (continuous if Ts==0).
Value ss(std::pmr::memory_resource *mr,
         const Value &A, const Value &B,
         const Value &C, const Value &D, double Ts);

/// `filt(num, den [, Ts])` — discrete tf with z^-1 variable convention.
/// Numerator and denominator coefficients are kept as-is; the only
/// difference vs `tf` is the default Ts (-1, "unspecified discrete")
/// and an informational `variable` field set to "z^-1".
Value filt(std::pmr::memory_resource *mr,
           const Value &num, const Value &den, double Ts);

/// `frd(response, frequency [, Ts])` — frequency-response data model.
/// Builds a struct {kind='frd', resp, freq, Ts}. `response` may be
/// complex; `frequency` is a real vector. Both stored as column
/// vectors to match MATLAB's convention.
Value frd(std::pmr::memory_resource *mr,
          const Value &response, const Value &frequency, double Ts);

/// `frdata(sys)` — extract response / frequency vectors from an frd.
std::tuple<Value, Value>
frdata(std::pmr::memory_resource *mr, const Value &sys);

/// `tfdata(sys[, 'v'])` — extract num/den. With 'v' returns numeric row
/// vectors padded so num and den have equal length (leading zeros on
/// num); without 'v', wraps each row vector in a 1×1 cell.
/// Accepts tf or zpk / ss inputs (the latter are converted via the
/// existing zp2tf / ss2tf paths).
std::tuple<Value, Value>
tfdata(std::pmr::memory_resource *mr, const Value &sys, bool asVector);

/// `zpkdata(sys[, 'v'])` — extract zeros / poles / gain. `z` and `p`
/// are returned as column vectors (or 1×1 cells without 'v'). `k` is
/// always a numeric scalar.
std::tuple<Value, Value, Value>
zpkdata(std::pmr::memory_resource *mr, const Value &sys, bool asVector);

/// `ssdata(sys)` — extract A, B, C, D matrices.
std::tuple<Value, Value, Value, Value>
ssdata(std::pmr::memory_resource *mr, const Value &sys);

} // namespace numkit::control
