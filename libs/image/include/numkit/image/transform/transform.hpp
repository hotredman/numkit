// libs/image/include/numkit/image/transform/transform.hpp
//
// Image transforms: 2-D DCT, integral images, checkerboard / phantom
// patterns, normalised cross-correlation, OTF/PSF conversions.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>
#include <string>
#include <tuple>

namespace numkit::image {

/// 2-D Type-II orthonormal discrete cosine transform (`B = dct2(A)`).
///
/// Separable: `dct2(A) == dct(dct(A).').'`. Internally applies the
/// 1-D `signal::dct` along columns first, then along rows.
///
/// @param A   2-D real matrix.
/// @param mr  Memory resource (nullptr → process default).
/// @return    M×N DCT coefficients of the same size as `A`.
///
/// @see idct2, dctmtx
Value dct2(const Value &A, std::pmr::memory_resource *mr = nullptr);

/// Inverse 2-D DCT (`A = idct2(B)`).
///
/// Exact left-inverse of @ref dct2.
Value idct2(const Value &B, std::pmr::memory_resource *mr = nullptr);

/// N×N DCT-II transform matrix (`D = dctmtx(N)`).
///
/// Returns an orthonormal matrix D (D'·D = I) such that `D * A`
/// applies the 1-D DCT to each column of `A`. Useful for block DCTs.
Value dctmtx(double N, std::pmr::memory_resource *mr = nullptr);

/// Summed-area table (`J = integralImage(I)`).
///
/// Output is (M+1)×(N+1) double with a leading zero row and column
/// so that the rectangle sum @f$ \sum_{r=r_0}^{r_1} \sum_{c=c_0}^{c_1} I @f$
/// equals
/// @f$ J[r_1+1, c_1+1] - J[r_0, c_1+1] - J[r_1+1, c_0] + J[r_0, c_0] @f$.
///
/// @see integralImage3
Value integralImage(const Value &I,
                    std::pmr::memory_resource *mr = nullptr);

/// 3-D summed-volume table (`J = integralImage3(V)`).
///
/// Output is (M+1)×(N+1)×(P+1) double with a leading zero plane,
/// row, and column. Direct 3-D extension of @ref integralImage.
Value integralImage3(const Value &V,
                     std::pmr::memory_resource *mr = nullptr);

/// Checkerboard test pattern (`I = checkerboard(side, M, N)`).
///
/// Produces a `2·M·side × 2·N·side` double image with alternating
/// black / white squares; the right half is dimmed to grey (×0.7).
/// Matches Octave-image's `checkerboard.m`.
///
/// Defaults if 0 / 0 / 0 are passed: side = 10, M = 4, N = 4.
Value checkerboard(size_t side, size_t M, size_t N,
                   std::pmr::memory_resource *mr = nullptr);

/// Normalised cross-correlation for template matching
/// (`C = normxcorr2(template, img)`).
///
/// Output is (M+m−1)×(N+n−1) double in [-1, 1] (numerical noise
/// outside this range is clamped to 0 by the inf/NaN guard).
/// Algorithm follows Octave-image's `normxcorr2.m`.
///
/// @param templ  Template image (m×n).
/// @param img    Search image (M×N), must be larger than `templ`.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Correlation map.
Value normxcorr2(const Value &templ, const Value &img,
                 std::pmr::memory_resource *mr = nullptr);

/// PSF → OTF (`OTF = psf2otf(PSF, outsize)`).
///
/// Pads `PSF` with zeros to `outsize`, circularly shifts by
/// @f$ -\lfloor \text{size}(PSF)/2 \rfloor @f$ so the PSF centre
/// lands at the origin, then applies 2-D FFT (or 1-D for vectors).
///
/// @param PSF      Point spread function.
/// @param outsize  Desired output size (vector); empty → same as PSF.
/// @param mr       Memory resource (nullptr → process default).
/// @return         Complex OTF.
///
/// @see otf2psf
Value psf2otf(const Value &PSF, const Value &outsize,
              std::pmr::memory_resource *mr = nullptr);

/// 2-D convolution via FFT (`Y = fftconv2(A, B, shape)`).
///
/// Faster than direct conv2 for large inputs, less accurate. `shape`
/// is `"full"` (default), `"same"`, or `"valid"`. Output is complex
/// (a tiny imaginary part appears even for real inputs).
Value fftconv2(const Value &A, const Value &B, const std::string &shape,
               std::pmr::memory_resource *mr = nullptr);

/// Best block size for block-wise processing (`siz = bestblk(IMS, k)`).
///
/// For each dim of `IMS`: keep it if it's ≤ k (default 100); else
/// scan from k down to `max(dim/10, k/2)` and pick the largest size
/// with the smallest mod-padding `mod(-dim, p)`.
Value bestblk(const Value &IMS, double k,
              std::pmr::memory_resource *mr = nullptr);

/// OTF → PSF (`PSF = otf2psf(OTF, outsize)`).
///
/// Inverse of @ref psf2otf: applies the inverse 2-D FFT, then
/// circularly shifts by `+floor(size(PSF)/2)` to recover the centred
/// PSF. Output is double if the imaginary part is negligible,
/// complex otherwise.
Value otf2psf(const Value &OTF, const Value &outsize,
              std::pmr::memory_resource *mr = nullptr);

/// Shepp–Logan computational head phantom (`[P, E] = phantom(model, n)`).
///
/// Generates the classic CT test image of the same name. `model_or_E`
/// is either:
///   - a string: `"Shepp-Logan"` or `"Modified Shepp-Logan"` (default).
///   - a matrix: N×6 of ellipse parameters
///     `{I, a, b, x0, y0, phi_deg}` to use directly.
///
/// @param model_or_E  String name or ellipse parameter matrix.
/// @param n           Output size (n × n).
/// @param mr          Memory resource (nullptr → process default).
/// @return            `(P, E)` — image and ellipse matrix actually used.
std::tuple<Value, Value>
phantom(const Value &model_or_E, size_t n,
        std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::image
