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
Value tf(const Value &num, const Value &den, double Ts, std::pmr::memory_resource *mr = nullptr);

/// `zpk(z, p, k [, Ts])` — zero-pole-gain form.
Value zpk(const Value &z, const Value &p, const Value &k, double Ts, std::pmr::memory_resource *mr = nullptr);

/// `ss(A, B, C, D [, Ts])` — state-space (continuous if Ts==0).
Value ss(const Value &A, const Value &B, const Value &C, const Value &D, double Ts, std::pmr::memory_resource *mr = nullptr);

/// `filt(num, den [, Ts])` — discrete tf with z^-1 variable convention.
/// Numerator and denominator coefficients are kept as-is; the only
/// difference vs `tf` is the default Ts (-1, "unspecified discrete")
/// and an informational `variable` field set to "z^-1".
Value filt(const Value &num, const Value &den, double Ts, std::pmr::memory_resource *mr = nullptr);

/// `frd(response, frequency [, Ts])` — frequency-response data model.
/// Builds a struct {kind='frd', resp, freq, Ts}. `response` may be
/// complex; `frequency` is a real vector. Both stored as column
/// vectors to match MATLAB's convention.
Value frd(const Value &response, const Value &frequency, double Ts, std::pmr::memory_resource *mr = nullptr);

/// `frdata(sys)` — extract response / frequency vectors from an frd.
std::tuple<Value, Value>
frdata(const Value &sys, std::pmr::memory_resource *mr = nullptr);

/// `tfdata(sys[, 'v'])` — extract num/den. With 'v' returns numeric row
/// vectors padded so num and den have equal length (leading zeros on
/// num); without 'v', wraps each row vector in a 1×1 cell.
/// Accepts tf or zpk / ss inputs (the latter are converted via the
/// existing zp2tf / ss2tf paths).
std::tuple<Value, Value>
tfdata(const Value &sys, bool asVector, std::pmr::memory_resource *mr = nullptr);

/// `zpkdata(sys[, 'v'])` — extract zeros / poles / gain. `z` and `p`
/// are returned as column vectors (or 1×1 cells without 'v'). `k` is
/// always a numeric scalar.
std::tuple<Value, Value, Value>
zpkdata(const Value &sys, bool asVector, std::pmr::memory_resource *mr = nullptr);

/// `ssdata(sys)` — extract A, B, C, D matrices.
std::tuple<Value, Value, Value, Value>
ssdata(const Value &sys, std::pmr::memory_resource *mr = nullptr);

/// `ss2ss(sys, T)` — similarity transform of a state-space model.
///   A' = T·A·T⁻¹,  B' = T·B,  C' = C·T⁻¹,  D' = D
/// Returns a new ss struct. T must be invertible.
Value ss2ss(const Value &sys, const Value &T, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::control
