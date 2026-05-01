// libs/signal/include/numkit/signal/transforms/extras.hpp
//
// Transform extras (E2): dftmtx, bitrevorder, dst, idst, rceps, cceps, icceps.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::signal {

/// dftmtx(N) — N×N complex DFT matrix, F[k][n] = exp(-2πi*k*n/N).
Value dftmtx(std::pmr::memory_resource *mr, size_t N);

/// bitrevorder(x) — re-order x by bit-reversed indices. Length must be
/// a power of 2; throws otherwise.
Value bitrevorder(std::pmr::memory_resource *mr, const Value &x);

/// dst(x) — Type-II Discrete Sine Transform of a real vector.
///   Y[k] = sum_{n=0}^{N-1} x[n] * sin(π*(n+1)*(k+1)/(N+1)),  k=0..N-1
Value dst(std::pmr::memory_resource *mr, const Value &x);

/// idst(y) — inverse of dst (Type-II DST is involutive up to a factor
/// of 2/(N+1)).
Value idst(std::pmr::memory_resource *mr, const Value &y);

/// rceps(x) — real cepstrum: real(ifft(log(abs(fft(x))))).
Value rceps(std::pmr::memory_resource *mr, const Value &x);

/// cceps(x) — complex cepstrum: ifft(log(fft(x))) with phase unwrap.
Value cceps(std::pmr::memory_resource *mr, const Value &x);

/// icceps(c) — inverse complex cepstrum: real(ifft(exp(fft(c)))).
Value icceps(std::pmr::memory_resource *mr, const Value &c);

} // namespace numkit::signal
