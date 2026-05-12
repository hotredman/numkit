// libs/signal/include/numkit/signal/transforms/extras.hpp
//
// Transform extras (E2): dftmtx, bitrevorder, dst, idst, rceps, cceps, icceps.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::signal {

/// dftmtx(N) — N×N complex DFT matrix, F[k][n] = exp(-2πi*k*n/N).
Value dftmtx(size_t N, std::pmr::memory_resource *mr = nullptr);

/// bitrevorder(x) — re-order x by bit-reversed indices. Length must be
/// a power of 2; throws otherwise.
Value bitrevorder(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// dst(x) — Type-II Discrete Sine Transform of a real vector.
///   Y[k] = sum_{n=0}^{N-1} x[n] * sin(π*(n+1)*(k+1)/(N+1)),  k=0..N-1
Value dst(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// idst(y) — inverse of dst (Type-II DST is involutive up to a factor
/// of 2/(N+1)).
Value idst(const Value &y, std::pmr::memory_resource *mr = nullptr);

/// rceps(x) — real cepstrum: real(ifft(log(abs(fft(x))))).
Value rceps(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// cceps(x) — complex cepstrum: ifft(log(fft(x))) with phase unwrap.
Value cceps(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// icceps(c) — inverse complex cepstrum: real(ifft(exp(fft(c)))).
Value icceps(const Value &c, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::signal
