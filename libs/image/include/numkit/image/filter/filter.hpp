// libs/image/include/numkit/image/filter/filter.hpp
//
// Image filtering primitives. Portable scalar; SIMD planned for a
// later phase.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <string>
#include <vector>

namespace numkit::image {

enum class PadMode {
    Constant,    // pad with `pad_value`
    Replicate,   // repeat edge pixel
    Symmetric,   // mirror without duplicating edge: a b c | b a
    Circular     // wrap-around
};

/// padarray(A, padsize[, mode|val][, direction]) — pad A by `padsize`
/// elements on each side. `padsize` is a 1-D vector with one entry per
/// dimension. Direction: "both" (default), "pre", "post".
Value padarray(std::pmr::memory_resource *mr, const Value &x,
               const std::vector<int> &padsize,
               PadMode mode, double pad_value,
               const std::string &direction);

/// fspecial(type, ...) — returns a 2-D filter kernel as a DOUBLE matrix.
/// Supported types: 'average', 'gaussian', 'laplacian', 'log',
/// 'sobel', 'prewitt', 'disk'.
Value fspecial(std::pmr::memory_resource *mr,
               const std::string &type,
               const std::vector<double> &params);

/// imfilter(I, h, [boundary, output_size, conv_or_corr])
///   boundary: 0 (default), 'replicate', 'symmetric', 'circular'
///   output_size: 'same' (default) or 'full'
///   conv_or_corr: 'corr' (default) or 'conv'
Value imfilter(std::pmr::memory_resource *mr,
               const Value &I, const Value &h,
               PadMode boundary, double pad_value,
               bool full, bool flip_kernel);

/// imgaussfilt(I, sigma[, FilterSize]) — 2-D Gaussian filtering with
/// boundary='replicate', output='same'.
Value imgaussfilt(std::pmr::memory_resource *mr,
                  const Value &I, double sigma, int filter_size);

/// imboxfilt(I, FilterSize) — local mean filter with replicate boundary.
Value imboxfilt(std::pmr::memory_resource *mr, const Value &I, int filter_size);

/// medfilt2(I[, [m n]]) — 2-D median filter. Default 3×3.
Value medfilt2(std::pmr::memory_resource *mr, const Value &I,
               int rows = 3, int cols = 3);

/// imsharpen(I[, radius, amount, threshold]) — unsharp masking.
///   high  = I − imgaussfilt(I, radius)
///   mask  = (|high| ≥ threshold·max|high|)
///   B     = saturate(I + amount · mask · high)
/// MATLAB defaults: radius=1, amount=0.8, threshold=0.
Value imsharpen(std::pmr::memory_resource *mr,
                const Value &I,
                double radius, double amount, double threshold);

/// im2col(A, [m n], block_type) — rearrange image neighborhoods into
/// matrix columns. block_type ∈ {"sliding", "distinct"} (default
/// "sliding"). Output is m·n × Ncols; same class as A. "distinct"
/// zero-pads when A's dims aren't multiples of m or n.
Value im2col(std::pmr::memory_resource *mr,
             const Value &A, int m, int n, const std::string &block_type);

/// col2im(B, [m n], [mm nn], block_type) — inverse of im2col.
///   "sliding"  expects B to be 1 × (mm−m+1)·(nn−n+1) (a row of
///              filter responses); reshapes to (mm−m+1) × (nn−n+1).
///   "distinct" expects B to be m·n × ⌈mm/m⌉·⌈nn/n⌉; rebuilds the
///              mm × nn image, dropping the zero-pad rim that
///              im2col added on the right/bottom edges.
Value col2im(std::pmr::memory_resource *mr,
             const Value &B, int m, int n, int mm, int nn,
             const std::string &block_type);

/// imbilatfilt(I, degreeOfSmoothing, spatialSigma) — bilateral
/// filter. Edge-preserving smoothing: weights combine a spatial
/// Gaussian (σ = spatialSigma) and a range Gaussian over intensity
/// difference (variance = degreeOfSmoothing). Boundary = replicate.
Value imbilatfilt(std::pmr::memory_resource *mr,
                  const Value &I,
                  double degreeOfSmoothing, double spatialSigma);

/// imnoise(I, mode[, p1, p2]) — additive / multiplicative noise.
/// Modes (all match MATLAB R2025b semantics):
///   "gaussian"        m=0, var=0.01      J = I + m + sqrt(var)·N(0,1)
///   "localvar"        V (variance map)   J = I + sqrt(V)·N(0,1)
///   "salt & pepper"   density d=0.05    fraction d of pixels set to {0,1}
///   "speckle"         var=0.04           J = I + I·N(0, var)
///   "poisson"         (no params)        Poisson with mean = I·scale
/// Image is treated as unit-range [0,1] internally; output is cast
/// back to the input class with saturation.
Value imnoise(std::pmr::memory_resource *mr,
              const Value &I, const std::string &mode,
              const Value &p1, const Value &p2);

/// stdfilt(I [, domain]) — local sample standard deviation in a
/// neighbourhood defined by `domain` (logical / numeric mask;
/// non-zero entries select neighbours). Default domain = ones(3).
/// Boundary handling = symmetric. Output is double.
Value stdfilt(std::pmr::memory_resource *mr,
              const Value &I, const Value &domain);

/// rangefilt(I [, domain]) — local max−min in a neighbourhood
/// defined by `domain`. Default domain = true(3). Boundary =
/// symmetric. Output type matches input class.
Value rangefilt(std::pmr::memory_resource *mr,
                const Value &I, const Value &domain);

/// `imsmooth(I, "Gaussian"[, sigma])` — Octave-image's smoothing
/// dispatcher. Currently supports only the "Gaussian" mode, with
/// 1-D-separable σ-Gaussian, h = ceil(3σ), default σ = 0.5,
/// symmetric boundary, computed in double then cast back to the
/// input class. RGB inputs are processed per-channel.
Value imsmooth(std::pmr::memory_resource *mr,
               const Value &I, const std::string &name,
               double sigma);

/// `J = imboxfilt3(V [, fH, fW, fP])` — 3-D box (mean) filter over
/// a volume. Per-axis filter sizes default to 3. Replicate
/// boundary on all 3 axes. Output type matches input.
Value imboxfilt3(std::pmr::memory_resource *mr, const Value &V,
                 int fH, int fW, int fP);

/// `T = convmtx2(h, m, n)` or `convmtx2(h, [m n])` — convolution
/// matrix for 2-D 'full' convolution. Output is dense
/// (m+M-1)*(n+N-1) by m*n where h is M×N. Multiplying T by
/// vec(I) (col-major) produces vec(conv2(I, h, 'full')).
/// MATLAB returns a sparse matrix; we return dense (numkit doesn't
/// have sparse). Output type is double regardless of input class.
Value convmtx2(std::pmr::memory_resource *mr,
               const Value &h, int m, int n);

/// `J = imgaussfilt3(V, sigmaH, sigmaW, sigmaP)` — 3-D Gaussian
/// filter applied via separable 1-D convolutions along each axis
/// with replicate boundary. Per-axis filter sizes default to
/// 2·ceil(2σ)+1. Output type matches input.
Value imgaussfilt3(std::pmr::memory_resource *mr, const Value &V,
                   double sigH, double sigW, double sigP);

/// `J = medfilt3(V[, [M N P]])` — 3-D median filter. Default
/// 3×3×3 neighborhood; sizes must be odd positive. Boundary is
/// symmetric (mirror reflection) per MATLAB R2025b default.
/// Output class matches input. NB: only 'symmetric' padopt is
/// supported in this first cut.
Value medfilt3(std::pmr::memory_resource *mr, const Value &V,
               int M, int N, int P);


/// entropyfilt(I [, domain]) — local Shannon entropy in bits.
/// 256-bin histogram for non-logical inputs (uint8 / im2uint8 cast),
/// 2 bins for logical. Default domain = ones(9). Symmetric boundary.
Value entropyfilt(std::pmr::memory_resource *mr,
                  const Value &I, const Value &domain);

/// ordfilt2(A, nth, domain [, S]) — 2-D order-statistic filter.
/// For each output pixel the neighbourhood selected by `domain`
/// (non-zero entries) is gathered, optionally offset by S (same
/// size as domain), sorted, and the `nth`-order element returned
/// (1-based). `boundary` controls padding of A when the
/// neighbourhood crosses the image edge. Output class matches A.
Value ordfilt2(std::pmr::memory_resource *mr,
               const Value &A, int nth, const Value &domain,
               const Value &S, PadMode boundary, double pad_value);

/// freqz2(h [, M, N]) — 2-D frequency response of filter h on a
/// freqspace-style M×N grid (default 64×64). Returns (H, f1, f2)
/// where H[i,j] = Σ_p Σ_q h[p,q] · exp(-iπ·(f1[i]·p + f2[j]·q)) and
/// f1 / f2 are the row/column frequency vectors from freqspace.
std::tuple<Value, Value, Value>
freqz2(std::pmr::memory_resource *mr, const Value &h, size_t M, size_t N);

/// wiener2(I [, nh, nw [, noise]]) — adaptive Wiener noise reduction
/// (Lim 1989, eq. 9.26-9.29). Local mean μ and variance σ² in an
/// nh×nw box neighbourhood (default 3×3, zero-pad boundary). When
/// `noise` is NaN it is estimated as the mean of σ² across the
/// image. Returns (denoised, noise) where `denoised` is cast back
/// to the input class.
std::tuple<Value, Value>
wiener2(std::pmr::memory_resource *mr, const Value &I,
        size_t nh, size_t nw, double noise);

} // namespace numkit::image
