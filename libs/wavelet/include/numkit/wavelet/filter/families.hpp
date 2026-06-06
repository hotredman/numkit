// libs/wavelet/include/numkit/wavelet/filter/families.hpp
//
// Family-named scaling filters (dbwavf / coifwavf / symwavf) and the
// orthogonal filter quadruple (orthfilt).

#pragma once

#include <memory_resource>
#include <string>
#include <numkit/value/value.hpp>

namespace numkit::wavelet {

/// @brief Daubechies scaling filter — `F = dbwavf(name)`.
///
/// Returns the Daubechies synthesis lowpass scaling filter `Lo_R / sqrt(2)`
/// (length 2N for `dbN`, sum = 1) as a row vector.
///
/// @param name  Wavelet name, `"haar"` or `"dbN"` (e.g. `"db4"`).
/// @param mr    Memory resource (nullptr → process default).
/// @return      Row vector of scaling-filter coefficients.
/// @throws Error if `name` is not `"haar"`/`"dbN"`, or the family is
///         unsupported by the underlying filter table.
/// @see coifwavf, symwavf, orthfilt
Value dbwavf(const std::string &name, std::pmr::memory_resource *mr = nullptr);

/// @brief Coiflet scaling filter — `F = coifwavf(name)`.
///
/// Returns the Coiflet synthesis lowpass scaling filter `Lo_R / sqrt(2)`
/// (length 6K for `coifK`) as a row vector.
///
/// @param name  Wavelet name `"coifK"` (e.g. `"coif2"`).
/// @param mr    Memory resource (nullptr → process default).
/// @return      Row vector of scaling-filter coefficients.
/// @throws Error if `name` is not `"coifK"`, or the family is unsupported.
/// @see dbwavf, symwavf, orthfilt
Value coifwavf(const std::string &name, std::pmr::memory_resource *mr = nullptr);

/// @brief Symlet scaling filter — `F = symwavf(name)`.
///
/// Returns the Symlet synthesis lowpass scaling filter `Lo_R / sqrt(2)`
/// (length 2N for `symN`) as a row vector.
///
/// @param name  Wavelet name `"symN"` (e.g. `"sym4"`).
/// @param mr    Memory resource (nullptr → process default).
/// @return      Row vector of scaling-filter coefficients.
/// @throws Error if `name` is not `"symN"`, or the family is unsupported.
/// @see dbwavf, coifwavf, orthfilt
Value symwavf(const std::string &name, std::pmr::memory_resource *mr = nullptr);

/// @brief The four orthogonal-wavelet filter banks produced by orthfilt.
struct OrthfiltResult {
    Value Lo_D;  ///< Decomposition (analysis) lowpass.
    Value Hi_D;  ///< Decomposition (analysis) highpass.
    Value Lo_R;  ///< Reconstruction (synthesis) lowpass.
    Value Hi_R;  ///< Reconstruction (synthesis) highpass.
};

/// @brief Orthogonal filter quadruple from a scaling filter —
/// `[Lo_D, Hi_D, Lo_R, Hi_R] = orthfilt(W)`.
///
/// Given a unit-normalised scaling filter `W` (`sum(W) = 1`), emits the four
/// orthogonal-wavelet filter banks via the standard QMF construction:
/// `Lo_R = W*sqrt(2)`, `Lo_D = reverse(Lo_R)`,
/// `Hi_R[k] = (-1)^k * Lo_R[N-1-k]`, `Hi_D = reverse(Hi_R)` — all row
/// vectors, in MATLAB's documented output order.
///
/// @param W   Scaling filter (non-empty; conventionally `sum(W) = 1`).
/// @param mr  Memory resource (nullptr → process default).
/// @return    @ref OrthfiltResult `{ Lo_D, Hi_D, Lo_R, Hi_R }`.
/// @throws Error if `W` is empty.
/// @see dbwavf, coifwavf, symwavf
OrthfiltResult orthfilt(const Value &W, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::wavelet
