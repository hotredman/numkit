// toolboxes/stats/include/numkit/stats/moving/moving.hpp
//
// Moving / sliding-window statistics.

#pragma once

#include <memory_resource>
#include <numkit/value/span.hpp>
#include <numkit/value/value.hpp>

#include <string>

namespace numkit::stats {

/// @file
/// @brief Moving / sliding-window statistics over a 1-D / N-D array.
///
/// **Window descriptor `k`** (shared by all `mov*` functions):
/// - 1-element span `{k}` → centred window of length `k`:
///   `kb = floor((k-1)/2)` leading samples, `kf = floor(k/2)` trailing
/// - 2-element span `{kb, kf}` → asymmetric window covering `[i-kb, i+kf]`
///
/// **Endpoints** are NOT discarded — the window truncates at the edges
/// ('shrink' is the default). Windows that become empty
/// after truncation produce `NaN`.
///
/// **Dimension argument `dim`**:
/// - `dim == 0` → operate along the first non-singleton dim (auto)
/// - `dim >= 1` → operate along that 1-based dim. Vector / scalar
///   inputs ignore `dim`.

/// @brief Moving mean along `dim` (`y = movmean(x, k)`).
///
/// For each output element `y[i]`, computes `mean(x[i-kb .. i+kf])`
/// where the window is determined by `k`. This is the
/// `movmean(x, k, dim)` operation.
///
/// @param x    Input array; any numeric type promoted to DOUBLE on read.
/// @param k    Window descriptor — see @ref moving.hpp file note.
/// @param dim  1-based dimension; 0 → first non-singleton dim.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Array of the same shape as `x` with the per-element mean.
/// @throws Error  Invalid window length (`m:movmean:badK`).
/// @see movsum, movmedian
Value movmean(const Value &x, Span<const size_t> k, int dim = 0,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Moving median along `dim` (`y = movmedian(x, k)`).
///
/// Per-window quickselect median. Window semantics as in @ref movmean.
///
/// @param x    Input array.
/// @param k    Window descriptor — see @ref moving.hpp file note.
/// @param dim  1-based dimension; 0 → first non-singleton dim.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Per-element moving median.
/// @throws Error  Invalid window length (`m:movmedian:badK`).
/// @see movmean, movmad
Value movmedian(const Value &x, Span<const size_t> k, int dim = 0,
                std::pmr::memory_resource *mr = nullptr);

/// @brief Moving sum along `dim` (`y = movsum(x, k)`).
///
/// @param x    Input array.
/// @param k    Window descriptor — see @ref moving.hpp file note.
/// @param dim  1-based dimension; 0 → first non-singleton dim.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Per-element moving sum.
/// @throws Error  Invalid window length (`m:movsum:badK`).
/// @see movmean, movprod
Value movsum(const Value &x, Span<const size_t> k, int dim = 0,
             std::pmr::memory_resource *mr = nullptr);

/// @brief Moving minimum along `dim` (`y = movmin(x, k)`).
///
/// @param x    Input array.
/// @param k    Window descriptor — see @ref moving.hpp file note.
/// @param dim  1-based dimension; 0 → first non-singleton dim.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Per-element moving minimum.
/// @throws Error  Invalid window length (`m:movmin:badK`).
/// @see movmax
Value movmin(const Value &x, Span<const size_t> k, int dim = 0,
             std::pmr::memory_resource *mr = nullptr);

/// @brief Moving maximum along `dim` (`y = movmax(x, k)`).
///
/// @param x    Input array.
/// @param k    Window descriptor — see @ref moving.hpp file note.
/// @param dim  1-based dimension; 0 → first non-singleton dim.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Per-element moving maximum.
/// @throws Error  Invalid window length (`m:movmax:badK`).
/// @see movmin
Value movmax(const Value &x, Span<const size_t> k, int dim = 0,
             std::pmr::memory_resource *mr = nullptr);

/// @brief Moving standard deviation along `dim` (`y = movstd(x, k, normFlag)`).
///
/// Computed as `sqrt(movvar(x, k, normFlag))`.
///
/// @param x         Input array.
/// @param k         Window descriptor — see @ref moving.hpp file note.
/// @param normFlag  Bias flag: `0` (default) divides by `n-1` (unbiased),
///                  `1` divides by `n` (population). Must be 0 or 1.
/// @param dim       1-based dimension; 0 → first non-singleton dim.
/// @param mr        Memory resource (nullptr → process default).
/// @return          Per-element moving standard deviation.
/// @throws Error  Invalid window length (`m:movstd:badK`) or invalid
///                normFlag (`m:movstd:badNormFlag`).
/// @see movvar, movmean
Value movstd(const Value &x, Span<const size_t> k, int normFlag = 0, int dim = 0,
             std::pmr::memory_resource *mr = nullptr);

/// @brief Moving variance along `dim` (`y = movvar(x, k, normFlag)`).
///
/// Same `normFlag` semantics as @ref movstd.
///
/// @param x         Input array.
/// @param k         Window descriptor — see @ref moving.hpp file note.
/// @param normFlag  Bias flag: `0` → divide by `n-1`, `1` → divide by `n`.
/// @param dim       1-based dimension; 0 → first non-singleton dim.
/// @param mr        Memory resource (nullptr → process default).
/// @return          Per-element moving variance.
/// @throws Error  Invalid window length (`m:movvar:badK`) or invalid
///                normFlag (`m:movvar:badNormFlag`).
/// @see movstd, movmean
Value movvar(const Value &x, Span<const size_t> k, int normFlag = 0, int dim = 0,
             std::pmr::memory_resource *mr = nullptr);

/// @brief Moving mean absolute deviation (`y = movmad(x, k)`).
///
/// Per window, computes `mean(|x[i] - median(window)|)`.
///
/// @param x    Input array.
/// @param k    Window descriptor — see @ref moving.hpp file note.
/// @param dim  1-based dimension; 0 → first non-singleton dim.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Per-element moving MAD.
/// @throws Error  Invalid window length (`m:movmad:badK`).
/// @see movmedian, movstd
Value movmad(const Value &x, Span<const size_t> k, int dim = 0,
             std::pmr::memory_resource *mr = nullptr);

/// @brief Moving product along `dim` (`y = movprod(x, k)`).
///
/// @param x    Input array.
/// @param k    Window descriptor — see @ref moving.hpp file note.
/// @param dim  1-based dimension; 0 → first non-singleton dim.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Per-element moving product.
/// @throws Error  Invalid window length (`m:movprod:badK`).
/// @see movsum
Value movprod(const Value &x, Span<const size_t> k, int dim = 0,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Method-dispatching data smoother (`y = smoothdata(x, method, k)`).
///
/// Routes to the requested moving-window kernel.
///
/// @param x       Input array.
/// @param method  One of:
///                - `"movmean"` (default) → @ref movmean
///                - `"movmedian"` → @ref movmedian
///                - `"gaussian"` → Gaussian-weighted mean with
///                  `sigma = (k-1)/4`
///                The `"lowess"`, `"loess"`, `"rlowess"`, `"rloess"`,
///                `"sgolay"` throw `m:smoothdata:unsupportedMethod`.
/// @param k       Centred window length. Pass `0` for the default
///                heuristic `max(min(round(0.1·n), 10), 3)`.
/// @param dim     1-based dimension; 0 → first non-singleton dim.
/// @param mr      Memory resource (nullptr → process default).
/// @return        Smoothed array, same shape as `x`.
/// @throws Error  Unsupported method (`m:smoothdata:unsupportedMethod`).
/// @see movmean, movmedian
Value smoothdata(const Value &x, const std::string &method = "movmean",
                 int k = 0, int dim = 0,
                 std::pmr::memory_resource *mr = nullptr);

/// @brief Hampel outlier-resilient median filter (`y = hampel(x, k, nsigmas)`).
///
/// Replaces each sample that deviates by more than `nsigmas · MAD` from
/// the local median (window of `2k+1` samples) with the local median
/// itself. The MAD→std conversion uses the standard `1.4826` constant.
///
/// @param x        1-D real input signal (vectors only; matrix form deferred).
/// @param k        Half-window length (full window is `2k+1`). Default 3.
/// @param nsigmas  Outlier threshold in robust-σ units. Default 3.0.
/// @param mr       Memory resource (nullptr → process default).
/// @return         Cleaned signal of the same length as `x`.
/// @throws Error  `k < 0` (`m:hampel:badK`), `nsigmas <= 0`
///                (`m:hampel:badSigmas`), or non-vector input
///                (`m:hampel:notVector`).
Value hampel(const Value &x, int k = 3, double nsigmas = 3.0,
             std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::stats
