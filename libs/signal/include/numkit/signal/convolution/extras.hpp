// libs/signal/include/numkit/signal/convolution/extras.hpp
//
// Convolution / correlation extras (E1):
//   cconv(x, y[, n])        — circular convolution
//   convmtx(h, n)           — convolution matrix
//   xcorr2(A, B)            — 2-D cross-correlation
//   finddelay(x, y[, max])  — lag of max(xcorr(x, y))
//   alignsignals(x, y[, max]) — align two signals by zero-padding the
//                              earlier one (returns aligned x and y).

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <tuple>

namespace numkit::signal {

/// Circular (cyclic) convolution: out[k] = sum_n x[n] * y[(k-n) mod N].
/// If n is omitted, N = max(numel(x), numel(y)). Inputs are zero-padded
/// to length N first.
Value cconv(std::pmr::memory_resource *mr, const Value &x, const Value &y, size_t n = 0);

/// Convolution matrix: convmtx(h, n) returns an (n+len(h)-1)×n matrix
/// such that A * x == conv(h, x) for any column vector x of length n.
Value convmtx(std::pmr::memory_resource *mr, const Value &h, size_t n);

/// 2-D cross-correlation: xcorr2(A, B) returns a (rA+rB-1)×(cA+cB-1)
/// real matrix C[k,l] = sum_{i,j} A[i,j] * B[i-k+rA-1, j-l+cA-1].
Value xcorr2(std::pmr::memory_resource *mr, const Value &A, const Value &B);

/// Return the integer lag d such that x(t) ≈ y(t - d) (i.e. the index
/// at which 1-D xcorr(x, y) is maximised). max_lag caps the search
/// range; 0 means full-length search.
long finddelay(std::pmr::memory_resource *mr, const Value &x, const Value &y,
               long max_lag = 0);

/// Align x and y by zero-padding the leading signal. Returns
/// (xa, ya) — both same length, with the lag from finddelay applied.
std::tuple<Value, Value>
alignsignals(std::pmr::memory_resource *mr, const Value &x, const Value &y,
             long max_lag = 0);

} // namespace numkit::signal
