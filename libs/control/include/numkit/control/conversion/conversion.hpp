// libs/control/include/numkit/control/conversion/conversion.hpp
//
// Inter-form converters between tf / zpk / ss representations. These
// are the matrix forms that operate directly on the coefficient
// arrays (so a script can use them either with raw num/den/A/B/C/D
// or with struct-tagged tf/zpk/ss values).

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>
#include <utility>

namespace numkit::control {

/// Result of `tf2zp(num, den)` — MATLAB `[z, p, k] = tf2zp(num, den)`.
struct Tf2ZpResult {
    Value z;   ///< zeros (column vector)
    Value p;   ///< poles (column vector)
    Value k;   ///< gain  (scalar)
};

/// State-space realisation `(A, B, C, D)` — MATLAB `[A, B, C, D] = tf2ss(...)`.
struct StateSpace {
    Value A;
    Value B;
    Value C;
    Value D;
};

/// `[z, p, k] = tf2zp(num, den)` — find the zeros, poles and gain of
/// a SISO transfer function. Uses the existing builtin::roots solver.
Tf2ZpResult tf2zp(const Value &num, const Value &den,
                  std::pmr::memory_resource *mr = nullptr);

/// `[num, den] = zp2tf(z, p, k)` — expand back to coefficient form.
/// Uses the existing builtin::poly to expand the products.
std::pair<Value, Value>
zp2tf(const Value &z, const Value &p, const Value &k,
      std::pmr::memory_resource *mr = nullptr);

/// `[A, B, C, D] = tf2ss(num, den)` — controllable canonical form
/// (the SS realisation MATLAB returns by default).
/// num must be no longer than den (proper rational function).
StateSpace tf2ss(const Value &num, const Value &den,
                 std::pmr::memory_resource *mr = nullptr);

/// `[num, den] = ss2tf(A, B, C, D [, iu])` — Faddeev–LeVerrier
/// expansion of C·adj(sI−A)·B + D·det(sI−A). `iu` selects the input
/// column when D has multiple inputs (1-based, default 1).
std::pair<Value, Value>
ss2tf(const Value &A, const Value &B,
      const Value &C, const Value &D, int iu,
      std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::control
