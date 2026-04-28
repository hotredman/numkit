// libs/builtin/include/numkit/m/builtin/MStdNumberTheory.hpp
//
// Elementary number-theory builtins: primes / isprime / factor.
// All operate on real, non-negative integer arguments.

#pragma once

#include <numkit/m/core/MAllocator.hpp>
#include <numkit/m/core/MValue.hpp>

namespace numkit::m::builtin {

/// primes(n) — row vector of all primes ≤ n. n < 2 → empty 1×0 row.
/// Sieve of Eratosthenes; output type DOUBLE (matches MATLAB).
MValue primes(Allocator &alloc, double n);

/// isprime(x) — element-wise primality. LOGICAL output, same shape
/// as x. Non-integer / negative / NaN entries → false.
MValue isprime(Allocator &alloc, const MValue &x);

/// factor(n) — prime-factor decomposition. Returns a row vector of
/// primes whose product is n (with multiplicity). MATLAB conventions:
///   factor(0) → [0]
///   factor(1) → [1]
///   n must be a non-negative integer scalar; otherwise throws.
MValue factor(Allocator &alloc, double n);

} // namespace numkit::m::builtin
