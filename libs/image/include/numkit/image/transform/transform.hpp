// libs/image/include/numkit/image/transform/transform.hpp

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

namespace numkit::image {

/// dct2(A) — 2-D discrete cosine transform (Type-II, orthonormal). The DCT is
/// separable, so dct2(A) == dct(dct(A).').'  Internally we apply the 1-D
/// signal::dct along columns first, then along rows.
Value dct2(std::pmr::memory_resource *mr, const Value &A);

/// idct2(B) — inverse 2-D DCT.
Value idct2(std::pmr::memory_resource *mr, const Value &B);

/// dctmtx(N) — N×N DCT-II transform matrix D such that D*A applies the
/// 1-D DCT to each column of A. D'*D == eye(N) (orthonormal).
Value dctmtx(std::pmr::memory_resource *mr, double N);

/// integralImage(I) — summed-area table. Output is (M+1)×(N+1) double
/// with a leading zero row and column so that the rectangle sum
/// Σ I[r0..r1, c0..c1] equals
///   J[r1+1, c1+1] - J[r0, c1+1] - J[r1+1, c0] + J[r0, c0].
Value integralImage(std::pmr::memory_resource *mr, const Value &I);

/// integralImage3(V) — 3-D summed-volume table. Output is
/// (M+1)×(N+1)×(P+1) double with leading zero plane / row / column.
Value integralImage3(std::pmr::memory_resource *mr, const Value &V);

/// checkerboard(side, M, N) — 2*M*side × 2*N*side double image with
/// alternating black / white squares; the right half is dimmed to
/// grey (×0.7). Defaults: side=10, M=4, N=4. Matches Octave-image's
/// checkerboard.m.
Value checkerboard(std::pmr::memory_resource *mr,
                   size_t side, size_t M, size_t N);

/// normxcorr2(template, img) — normalized cross-correlation, mostly
/// used for template matching. Output is (M+m-1)×(N+n-1) double in
/// [-1, 1] (numerical noise outside this range clamped to 0 via the
/// inf/nan guard). Algorithm follows Octave-image normxcorr2.m.
Value normxcorr2(std::pmr::memory_resource *mr,
                 const Value &templ, const Value &img);

/// `otf = psf2otf(PSF [, outsize])` — Optical Transfer Function
/// from a Point Spread Function. Pads PSF with zeros to `outsize`,
/// circularly shifts by `-floor(size(PSF)/2)` so the PSF center
/// lands at the origin, then applies fft2 (2-D) or fft (1-D).
/// Output is complex.
Value psf2otf(std::pmr::memory_resource *mr,
              const Value &PSF, const Value &outsize);

/// `Y = fftconv2(A, B [, shape])` — 2-D convolution computed via
/// the FFT. Faster but less accurate than direct conv2 for large
/// inputs. shape ∈ {"full" (default), "same", "valid"}. Output is
/// complex (a small imaginary part appears even for real inputs).
Value fftconv2(std::pmr::memory_resource *mr,
               const Value &A, const Value &B,
               const std::string &shape);

/// `psf = otf2psf(OTF [, outsize])` — inverse: ifft2/ifft, then
/// circularly shift by `+floor(size(PSF)/2)` to recover the
/// centered PSF. Output is double if the imaginary part is
/// negligible, complex otherwise.
Value otf2psf(std::pmr::memory_resource *mr,
              const Value &OTF, const Value &outsize);

/// phantom([model | E] [, n]) — Shepp-Logan computational head
/// phantom. `model` is "Shepp-Logan" or "Modified Shepp-Logan"
/// (default). `E` is an N×6 matrix of ellipse parameters
/// {I, a, b, x0, y0, phi_deg}. Output is n×n double.
/// `ellipses_used` returns the parameter matrix actually used.
std::tuple<Value, Value>
phantom(std::pmr::memory_resource *mr,
        const Value &model_or_E, size_t n);

} // namespace numkit::image
