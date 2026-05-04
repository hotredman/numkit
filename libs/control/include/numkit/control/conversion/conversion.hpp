// libs/control/include/numkit/control/conversion/conversion.hpp
//
// Inter-form converters between tf / zpk / ss representations. These
// are the matrix forms that operate directly on the coefficient
// arrays (so a script can use them either with raw num/den/A/B/C/D
// or with struct-tagged tf/zpk/ss values).

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::control {

/// `[z, p, k] = tf2zp(num, den)` — find the zeros, poles and gain of
/// a SISO transfer function. Uses the existing builtin::roots solver.
void tf2zp(std::pmr::memory_resource *mr,
           const Value &num, const Value &den,
           Value *z, Value *p, Value *k);

/// `[num, den] = zp2tf(z, p, k)` — expand back to coefficient form.
/// Uses the existing builtin::poly to expand the products.
void zp2tf(std::pmr::memory_resource *mr,
           const Value &z, const Value &p, const Value &k,
           Value *num, Value *den);

/// `[A, B, C, D] = tf2ss(num, den)` — controllable canonical form
/// (the SS realisation MATLAB returns by default).
/// num must be no longer than den (proper rational function).
void tf2ss(std::pmr::memory_resource *mr,
           const Value &num, const Value &den,
           Value *A, Value *B, Value *C, Value *D);

/// `[num, den] = ss2tf(A, B, C, D [, iu])` — Faddeev–LeVerrier
/// expansion of C·adj(sI−A)·B + D·det(sI−A). `iu` selects the input
/// column when D has multiple inputs (1-based, default 1).
void ss2tf(std::pmr::memory_resource *mr,
           const Value &A, const Value &B,
           const Value &C, const Value &D, int iu,
           Value *num, Value *den);

} // namespace numkit::control
