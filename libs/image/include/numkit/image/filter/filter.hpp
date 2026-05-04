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

} // namespace numkit::image
