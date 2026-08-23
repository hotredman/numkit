/// @file transform.hpp
/// @ingroup group_image
// toolboxes/image/include/numkit/image/transform/transform.hpp
//
// Image transforms: 2-D DCT, integral images, checkerboard / phantom
// patterns, normalised cross-correlation, OTF / PSF conversions.

#pragma once

#include <memory_resource>
#include <numkit/value/span.hpp>
#include <numkit/value/value.hpp>
#include <string>
#include <tuple>

namespace numkit::image {

/// @brief 2-D Type-II orthonormal DCT (`B = dct2(A)`).
///
/// Separable: `dct2(A) == dct(dct(A).').'`. Internally applies the
/// 1-D `signal::dct` along columns first, then along rows.
///
/// @param A   2-D real matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `M × N` DCT coefficients of the same size as `A`.
/// @see idct2, dctmtx
Value dct2(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse 2-D DCT (`A = idct2(B)`).
///
/// Exact left-inverse of @ref dct2.
///
/// @param B   2-D DCT coefficients.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Reconstructed `M × N` matrix.
/// @see dct2
Value idct2(const Value &B, std::pmr::memory_resource *mr = nullptr);

/// @brief `N × N` DCT-II transform matrix (`D = dctmtx(N)`).
///
/// Returns an orthonormal matrix `D` (`D' · D == I`) such that
/// `D * A` applies the 1-D DCT to each column of `A`. Useful for
/// block-DCT pipelines.
///
/// @param N   Block size.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `N × N` orthonormal matrix.
/// @see dct2
Value dctmtx(double N, std::pmr::memory_resource *mr = nullptr);

/// @brief Summed-area table (`J = integralImage(I)`).
///
/// Output is `(M+1) × (N+1)` DOUBLE with a leading zero row and column,
/// so the rectangle sum @f$ \sum_{r=r_0}^{r_1} \sum_{c=c_0}^{c_1} I @f$
/// equals
/// @f$ J[r_1+1, c_1+1] - J[r_0, c_1+1] - J[r_1+1, c_0] + J[r_0, c_0] @f$.
///
/// @param I   Input image.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(M+1) × (N+1)` integral image.
/// @see integralImage3
Value integralImage(const Value &I, std::pmr::memory_resource *mr = nullptr);

/// @brief 3-D summed-volume table (`J = integralImage3(V)`).
///
/// Output is `(M+1) × (N+1) × (P+1)` DOUBLE with a leading zero plane,
/// row, and column. Direct 3-D extension of @ref integralImage.
///
/// @param V   Input volume.
/// @param mr  Memory resource (nullptr → process default).
/// @return    `(M+1) × (N+1) × (P+1)` integral volume.
/// @see integralImage
Value integralImage3(const Value &V, std::pmr::memory_resource *mr = nullptr);

/// @brief Checkerboard test pattern (`I = checkerboard(side, M, N)`).
///
/// Produces a `2·M·side × 2·N·side` DOUBLE image with alternating
/// black / white squares; the right half is dimmed to grey (×0.7).
/// Matches Octave-image's `checkerboard.m`. Pass `0` to any arg to
/// use defaults: `side = 10`, `M = 4`, `N = 4`.
///
/// @param side  Square edge length in pixels (0 → default 10).
/// @param M     Vertical block count (0 → default 4).
/// @param N     Horizontal block count (0 → default 4).
/// @param mr    Memory resource (nullptr → process default).
/// @return      `2·M·side × 2·N·side` checkerboard.
Value checkerboard(size_t side, size_t M, size_t N,
                   std::pmr::memory_resource *mr = nullptr);

/// @brief Normalised cross-correlation (`C = normxcorr2(template, img)`).
///
/// Output is `(M+m-1) × (N+n-1)` DOUBLE in `[-1, 1]` (numerical noise
/// outside this range is clamped to 0 by the inf/NaN guard).
/// Algorithm follows Octave-image's `normxcorr2.m`.
///
/// @param templ  Template image (`m × n`).
/// @param img    Search image (`M × N`, must be larger than `templ`).
/// @param mr     Memory resource (nullptr → process default).
/// @return       Correlation map.
Value normxcorr2(const Value &templ, const Value &img,
                 std::pmr::memory_resource *mr = nullptr);

/// @brief PSF → OTF (`OTF = psf2otf(PSF, outsize)`).
///
/// Pads `PSF` with zeros to `outsize`, circularly shifts by
/// @f$ -\lfloor \text{size}(PSF) / 2 \rfloor @f$ so the PSF centre
/// lands at the origin, then applies 2-D FFT (or 1-D for vectors).
///
/// @param PSF      Point spread function.
/// @param outsize  Desired output size; empty span → same as `PSF`.
/// @param mr       Memory resource (nullptr → process default).
/// @return         COMPLEX OTF.
/// @see otf2psf
Value psf2otf(const Value &PSF, Span<const size_t> outsize = {},
              std::pmr::memory_resource *mr = nullptr);

/// @brief 2-D convolution via FFT (`Y = fftconv2(A, B, shape)`).
///
/// Faster than direct `conv2` for large inputs, less accurate. Output
/// is COMPLEX (a tiny imaginary part appears even for real inputs).
///
/// @param A      First input.
/// @param B      Second input.
/// @param shape  `"full"` (default), `"same"`, or `"valid"`.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Convolution result.
/// @throws Error  Unknown `shape` (`m:fftconv2:shape`).
Value fftconv2(const Value &A, const Value &B, const std::string &shape,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Wiener deconvolution (`J = deconvwnr(I, PSF, NSR)` /
/// `deconvwnr(I, PSF, NCORR, ICORR)`).
///
/// Deblurs image `I` assuming a known point-spread function `PSF`.
/// The Wiener inverse filter
///   `G(k) = conj(H(k)) / (|H(k)|^2 + S_u/S_x)`
/// minimises the mean-square error between the estimated and the
/// true images (Gonzalez & Woods, *Digital Image Processing*).
/// `H(k)` is the OTF returned by @ref psf2otf at `size(I)`.
///
/// **Argument forms:**
///   * `(I, PSF, nsr_scalar)` — scalar NSR (`S_u/S_x`); `0`
///     produces the ideal inverse filter (subject to the small
///     `sqrt(eps)` denominator floor).
///   * `(I, PSF, ncorr, icorr)` — scalar noise / signal powers;
///     equivalent to `NSR = ncorr / icorr`.
///   * Array `NCORR` / `ICORR` (autocorrelation functions of the
///     same size as `I`) are also supported: each is FFT'd to a
///     power spectrum first; the 1-D extrapolation form of
///     MATLAB's `powerSpectrumFromACF` is not implemented and
///     throws.
///
/// Output class equals input class (uint8/uint16 are scaled by
/// the class range and saturating-cast at the end). Real inputs
/// produce a real output (the tiny imaginary part of the IFFT is
/// discarded).
///
/// @param I        2-D or 3-D blurred image (any numeric class).
/// @param PSF      Point-spread function (real, any size ≤ `I`).
/// @param nsr      Noise-to-signal power ratio (default 0).
/// @param mr       Memory resource (nullptr → process default).
/// @return         Deblurred image, same class and shape as `I`.
Value deconvwnr(const Value &I, const Value &PSF, double nsr,
                std::pmr::memory_resource *mr = nullptr);
/// @brief Wiener deconvolution using autocorrelation functions (`J = deconvwnr(I, PSF, NCORR, ICORR)`).
/// @param I Blurred input image.
/// @param PSF Point-spread function.
/// @param ncorr Autocorrelation function of noise.
/// @param icorr Autocorrelation function of original image.
/// @param mr Memory resource for output allocation.
/// @return Deblurred image.
Value deconvwnr(const Value &I, const Value &PSF,
                const Value &ncorr, const Value &icorr,
                std::pmr::memory_resource *mr = nullptr);

/// @brief Result of regularized deconvolution (`[J, LAGRA] = deconvreg(...)`).
struct DeconvregResult {
    Value  J;     ///< Deblurred restored image.
    double lagra; ///< Lagrange multiplier λ.
};

/// @brief Tikhonov-regularized deconvolution (`[J, LAGRA] = deconvreg(I, PSF, NP, ...)`)
///
/// Solves the constrained least-squares restoration problem.
///
/// @param I 2-D blurred image.
/// @param PSF Point-spread function.
/// @param np Noise power estimate.
/// @param lo Lower bound for Lagrange multiplier search.
/// @param hi Upper bound for Lagrange multiplier search.
/// @param regop Custom regularization operator (or empty for Laplacian).
/// @param mr Memory resource for output allocation.
/// @return DeconvregResult containing restored image `J` and Lagrange multiplier `lagra`.
DeconvregResult deconvreg(const Value &I, const Value &PSF,
                          double np, double lo, double hi,
                          const Value &regop,
                          std::pmr::memory_resource *mr = nullptr);

/// @brief Taper image edges via blur (`J = edgetaper(I, PSF)`).
///
/// Reduces FFT ringing in deblurring methods that use the DFT
/// (deconvwnr, deconvreg, deconvlucy). The output is a weighted
/// blend of `I` and `imfilter(I, PSF, 'circular')`:
///   `J = alpha .* I + (1 − alpha) .* blurredI`
/// where `alpha` is built from per-dimension PSF projections'
/// autocorrelation, equalling 1 in the interior and tapering to 0
/// at the edges over a width controlled by `PSF`.
///
/// Algorithm (transliterated from MATLAB R2025b `edgetaper.m`):
///   1. For each non-singleton dim `n`:
///        `proj   = sum(PSF along all other dims)`
///        `Z      = |fft(proj, sizeI(n) - 1)|^2`
///        `beta_n = real(ifft(Z))`
///        `beta_n = [beta_n  beta_n(1)] / max(beta_n)`   (length sizeI(n))
///   2. `alpha = outer-product of (1 − beta_n)` across the
///      non-singleton dims.
///   3. `blurredI = real(ifftn(fftn(I) .* psf2otf(PSF, sizeI)))`.
///   4. `J = alpha .* I + (1 − alpha) .* blurredI`, clipped to
///      `[min(I), max(I)]`.
///   5. Cast back to `class(I)` for integer types.
///
/// Constraints: `PSF` cannot exceed half of `I` in any dimension.
/// PSF must contain at least one non-zero element. 2-D and 1-D
/// inputs supported (the 3-D-and-up form is rarely used but the
/// MATLAB source documents it via the same algorithm; we throw a
/// clear error if `I` is 3-D so the caller knows to slice).
///
/// @param I    Input image.
/// @param PSF  Point-spread function (any size ≤ size(I)/2).
/// @param mr   Memory resource (nullptr → process default).
/// @return     Tapered image, same class and shape as `I`.
Value edgetaper(const Value &I, const Value &PSF,
                std::pmr::memory_resource *mr = nullptr);

/// @brief Best block size for block-wise processing
/// (`siz = bestblk(IMS, k)`).
///
/// For each dim of `IMS`: keep it if it's `<= k` (default 100); else
/// scan from `k` down to `max(dim/10, k/2)` and pick the largest size
/// with the smallest mod-padding `mod(-dim, p)`.
///
/// @param IMS  Image size vector.
/// @param k    Maximum block size (default 100).
/// @param mr   Memory resource (nullptr → process default).
/// @return     Best block size vector.
Value bestblk(const Value &IMS, double k,
              std::pmr::memory_resource *mr = nullptr);

/// @brief OTF → PSF (`PSF = otf2psf(OTF, outsize)`).
///
/// Inverse of @ref psf2otf: applies the inverse 2-D FFT, then
/// circularly shifts by `+floor(size(PSF) / 2)` to recover the centred
/// PSF. Output is DOUBLE if the imaginary part is negligible, COMPLEX
/// otherwise.
///
/// @param OTF      Optical transfer function.
/// @param outsize  Desired output size; empty span → no crop.
/// @param mr       Memory resource (nullptr → process default).
/// @return         PSF (DOUBLE or COMPLEX).
/// @see psf2otf
Value otf2psf(const Value &OTF, Span<const size_t> outsize = {},
              std::pmr::memory_resource *mr = nullptr);

/// @brief Shepp-Logan computational head phantom
/// (`[P, E] = phantom(model_or_E, n)`).
///
/// Generates the classic CT test image.
///
/// @param model_or_E  Either a string `"Shepp-Logan"` /
///                    `"Modified Shepp-Logan"` (default), or an
///                    `N × 6` matrix of ellipse parameters
///                    `{I, a, b, x0, y0, phi_deg}`.
/// @param n           Output size (`n × n`).
/// @param mr          Memory resource (nullptr → process default).
/// @return            `(P, E)` — image and ellipse matrix actually used.
std::tuple<Value, Value>
phantom(const Value &model_or_E, size_t n,
        std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::image
