// libs/signal/include/numkit/signal/filter_analysis/responses.hpp
//
// Time- and phase-domain response helpers around the (b, a) digital-
// filter representation. Built on the existing filter() / freqz()
// kernels — no new SIMD here.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::signal {

/// impz(b, a[, n]) — impulse response of H(z) = B(z)/A(z), length n.
/// When n is 0, impzlength() chooses a reasonable default. Returns
/// (h, t) where t is the zero-based sample index column vector.
std::tuple<Value, Value>
impz(const Value &b, const Value &a, size_t n = 0, std::pmr::memory_resource *mr = nullptr);

/// impzlength(b, a) — heuristic count of significant impulse-response
/// samples. For FIR (a is scalar 1) returns numel(b). For IIR uses
/// `max(50, ceil(-log(1e-5)/log(max_pole_radius)))` clipped to [50, 8192].
size_t impzlength(const Value &b, const Value &a, std::pmr::memory_resource *mr = nullptr);

/// stepz(b, a[, n]) — step response: filter(b, a, ones(n,1)). Returns
/// (s, t). Default n picked by impzlength.
std::tuple<Value, Value>
stepz(const Value &b, const Value &a, size_t n = 0, std::pmr::memory_resource *mr = nullptr);

/// phasedelay(b, a[, n]) — phase delay = -phase(H)/omega. Returns
/// (pd, w) for w in [0, π]. Phase is unwrapped before the divide; the
/// w=0 sample uses the next sample to dodge a 0/0.
std::tuple<Value, Value>
phasedelay(const Value &b, const Value &a, size_t n = 512, std::pmr::memory_resource *mr = nullptr);

/// zerophase(b, a[, n]) — equivalent zero-phase response: complex H
/// with the linear-phase delay component removed. For symmetric or
/// antisymmetric FIR filters this returns a real-valued response that
/// can be negative. Returns (Hr, w).
std::tuple<Value, Value>
zerophase(const Value &b, const Value &a, size_t n = 512, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::signal
