// toolboxes/signal/include/numkit/signal/transforms/extras.hpp
//
// Transform extras (E2): dftmtx, bitrevorder, dst, idst, rceps, cceps, icceps.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <tuple>
#include <utility>

namespace numkit::signal {

/// N×N complex DFT matrix.
///
/// Returns the matrix \f$ F \f$ with
/// \f$ F[k][n] = \exp(-2\pi j \cdot k \cdot n / N) \f$,
/// so that for any column vector `x`, `F * x == fft(x, N)`.
///
/// @param N   Matrix dimension, ≥ 1.
/// @param mr  Memory resource (nullptr → process default).
/// @return    N×N COMPLEX matrix.
///
/// @code  Value F = dftmtx(8);  Value X = F * x;  // FFT via matmul  @endcode
Value dftmtx(size_t N, std::pmr::memory_resource *mr = nullptr);

/// Re-order a sequence by bit-reversed indices.
///
/// For a length-N vector with N a power of 2, output[k] = input[brev(k)],
/// where `brev(k)` reverses the bits of the `log2(N)`-bit index.
///
/// Used as a pre/post-step in some FFT decimation-in-frequency algorithms.
///
/// @param x   Real or complex input vector. Length must be a power of 2.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Same-shape Value with elements permuted.
/// @throws    numkit::Error  if `numel(x)` is not a power of 2.
Value bitrevorder(const Value &                x,
                  std::pmr::memory_resource *  mr = nullptr);

/// Type-II Discrete Sine Transform.
///
/// Computes
/// \f$ Y[k] = \sum_{n=0}^{N-1} x[n] \cdot \sin\!\left(\frac{\pi (n+1)(k+1)}{N+1}\right) \f$
/// for k = 0..N-1.
///
/// @param x   Real input vector.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Same-length DOUBLE vector.
///
/// @see idst, dct
Value dst(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// Inverse Type-II DST.
///
/// Type-II DST is involutive up to the factor `2/(N+1)`. This function
/// returns the properly-scaled inverse.
///
/// @param y   Real input vector (output of dst).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Same-length DOUBLE vector.
///
/// @see dst
Value idst(const Value &y, std::pmr::memory_resource *mr = nullptr);

/// Real cepstrum: `real(ifft(log(abs(fft(x)))))`.
///
/// The real cepstrum is used in homomorphic signal processing for
/// echo detection and pitch estimation.
///
/// @param x   Real input vector.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Same-length DOUBLE vector.
///
/// @see cceps, icceps
Value rceps(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// Real cepstrum and its minimum-phase reconstruction
/// (`[y, ym] = rceps(x)`).
///
/// `y` is the real cepstrum (as @ref rceps); `ym` is the minimum-phase
/// signal whose real cepstrum equals `y`, obtained by folding the cepstrum
/// with the window `[1, 2,…,2, (1 if length even), 0,…,0]` and running it
/// back through `real(ifft(exp(fft(·))))`.
///
/// @param x   Real input vector.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(y, ym)` pair, each the same length/orientation as `x`.
/// @see rceps, cceps
std::pair<Value, Value> rcepsMinPhase(const Value &x,
                                      std::pmr::memory_resource *mr = nullptr);

/// Complex cepstrum: `ifft(log(fft(x)))` with phase unwrapping.
///
/// Unlike `rceps`, preserves both magnitude and phase information,
/// enabling exact inversion via `icceps`.
///
/// @param x     Real input vector.
/// @param mr    Memory resource (nullptr → process default).
/// @param ndOut If non-null, receives `nd` — the integer (circular) delay
///              removed by the rcunwrap linear-phase term, i.e. the 2nd output
///              of MATLAB `[xhat, nd] = cceps(x)`.
/// @return    Same-length DOUBLE vector.
///
/// @see icceps, rceps
Value cceps(const Value &x, std::pmr::memory_resource *mr = nullptr,
            double *ndOut = nullptr);

/// Inverse complex cepstrum: `real(ifft(exp(fft(c))))`.
///
/// Recovers the original signal from a complex cepstrum produced by
/// `cceps`.
///
/// @param c   Complex cepstrum.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Same-length DOUBLE vector.
///
/// @see cceps
Value icceps(const Value &c, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::signal
