// libs/image/include/numkit/image/filter/filter.hpp
//
// Image filtering primitives. Portable scalar; SIMD planned for a
// later phase.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <string>
#include <tuple>
#include <vector>

namespace numkit { class Engine; }

namespace numkit::image {

/// @brief Boundary handling for image filtering / padding.
enum class PadMode {
    Constant,    ///< pad with `pad_value`
    Replicate,   ///< repeat edge pixel
    Symmetric,   ///< mirror without duplicating edge: `a b c | b a`
    Circular     ///< wrap-around
};

/// @brief Pad an array by `padsize` elements on each side
/// (`B = padarray(A, padsize, mode, val, dir)`).
///
/// `padsize` is a 1-D vector with one entry per dimension. Direction
/// controls which side(s) to pad:
/// - `"both"` (default): pad both sides.
/// - `"pre"`:  pad before each dimension.
/// - `"post"`: pad after each dimension.
///
/// @param x         Input array.
/// @param padsize   Per-dimension pad widths.
/// @param mode      @ref PadMode.
/// @param pad_value Constant value (used iff mode == Constant).
/// @param direction `"both"` / `"pre"` / `"post"`.
/// @param mr        Memory resource (nullptr → process default).
/// @return          Padded array.
Value padarray(const Value &x, const std::vector<int> &padsize,
               PadMode mode, double pad_value,
               const std::string &direction,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Build a 2-D filter kernel by name
/// (`h = fspecial(type, params)`).
///
/// Supported types: `"average"`, `"gaussian"`, `"laplacian"`, `"log"`,
/// `"sobel"`, `"prewitt"`, `"disk"`. The `params` interpretation
/// depends on `type` (e.g. `"gaussian"` takes `[hsize, sigma]`).
///
/// @param type    Kernel name (see list above).
/// @param params  Type-specific parameter vector.
/// @param mr      Memory resource (nullptr → process default).
/// @return        DOUBLE kernel matrix.
/// @throws Error  Unknown type or wrong-shape params.
Value fspecial(const std::string &type, const std::vector<double> &params,
               std::pmr::memory_resource *mr = nullptr);

/// @brief 2-D linear filtering (`B = imfilter(I, h, ...)`).
///
/// Direct convolution or correlation of `I` with kernel `h`.
///
/// @param I            Input image.
/// @param h            Filter kernel (any size).
/// @param boundary     @ref PadMode (Constant → use `pad_value`).
/// @param pad_value    Constant pad value.
/// @param full         `false` → output size = input ("same");
///                     `true`  → output size = input + kernel − 1 ("full").
/// @param flip_kernel  `true` → convolution (kernel flipped);
///                     `false` → correlation.
/// @param mr           Memory resource (nullptr → process default).
/// @return             Filtered image, same class as `I`.
Value imfilter(const Value &I, const Value &h,
               PadMode boundary, double pad_value,
               bool full, bool flip_kernel,
               std::pmr::memory_resource *mr = nullptr);

/// @brief 2-D Gaussian filter
/// (`B = imgaussfilt(I, sigma, filter_size)`).
///
/// Boundary handling: replicate; output size = input.
///
/// @param I            Input image.
/// @param sigma        Standard deviation in pixels.
/// @param filter_size  Filter side length (0 → default
///                     `2·ceil(2σ)+1`).
/// @param mr           Memory resource (nullptr → process default).
/// @return             Filtered image, same class as `I`.
/// @see imgaussfilt3, imboxfilt
Value imgaussfilt(const Value &I, double sigma, int filter_size,
                  std::pmr::memory_resource *mr = nullptr);

/// @brief 2-D box / mean filter (`B = imboxfilt(I, filter_size)`).
///
/// Replicate boundary. Constant-time per-pixel via integral images.
///
/// @param I            Input image.
/// @param filter_size  Square neighbourhood side length (odd).
/// @param mr           Memory resource (nullptr → process default).
/// @return             Filtered image, same class as `I`.
/// @see imgaussfilt, imboxfilt3
Value imboxfilt(const Value &I, int filter_size,
                std::pmr::memory_resource *mr = nullptr);

/// @brief 2-D median filter (`B = medfilt2(I, [m n])`).
///
/// Default neighbourhood 3×3. Symmetric boundary.
///
/// @param I     Input image.
/// @param rows  Neighbourhood rows. Default 3.
/// @param cols  Neighbourhood cols. Default 3.
/// @param mr    Memory resource (nullptr → process default).
/// @return      Filtered image, same class as `I`.
/// @see medfilt3, ordfilt2
Value medfilt2(const Value &I, int rows = 3, int cols = 3,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Unsharp masking
/// (`B = imsharpen(I, radius, amount, threshold)`).
///
/// `high  = I - imgaussfilt(I, radius)`;
/// `mask  = |high| ≥ threshold · max|high|`;
/// `B     = saturate(I + amount · mask · high)`.
///
/// Defaults: `radius = 1`, `amount = 0.8`, `threshold = 0`.
///
/// @param I          Input image.
/// @param radius     Gaussian radius in pixels.
/// @param amount     Sharpening strength (positive scalar).
/// @param threshold  Threshold on |high| relative to its max.
/// @param mr         Memory resource (nullptr → process default).
/// @return           Sharpened image, same class as `I`.
Value imsharpen(const Value &I, double radius, double amount, double threshold,
                std::pmr::memory_resource *mr = nullptr);

/// @brief Rearrange image neighbourhoods into matrix columns
/// (`B = im2col(A, [m n], block_type)`).
///
/// - `"sliding"` (default): every m×n window of A becomes a column.
///   Output is `m·n × (M-m+1)·(N-n+1)`.
/// - `"distinct"`: non-overlapping tiles, zero-padded when A's dims
///   aren't multiples of m or n.
///
/// Output class matches `A`.
///
/// @param A           Input image.
/// @param m           Window row count.
/// @param n           Window column count.
/// @param block_type  `"sliding"` (default) or `"distinct"`.
/// @param mr          Memory resource (nullptr → process default).
/// @return            Column-stacked neighbourhood matrix.
/// @see col2im
Value im2col(const Value &A, int m, int n, const std::string &block_type,
             std::pmr::memory_resource *mr = nullptr);

/// @brief Inverse of @ref im2col
/// (`A = col2im(B, [m n], [mm nn], block_type)`).
///
/// - `"sliding"` expects `B` to be `1 × (mm−m+1)·(nn−n+1)` (a row of
///   filter responses); reshapes to `(mm−m+1) × (nn−n+1)`.
/// - `"distinct"` expects `B` to be `m·n × ⌈mm/m⌉·⌈nn/n⌉`; rebuilds
///   the mm × nn image, dropping the zero-pad rim that @ref im2col
///   added on the right / bottom edges.
///
/// @param B           Neighbourhood-column matrix from @ref im2col.
/// @param m           Window row count used at packing time.
/// @param n           Window column count used at packing time.
/// @param mm          Target image row count.
/// @param nn          Target image column count.
/// @param block_type  `"sliding"` (default) or `"distinct"`.
/// @param mr          Memory resource (nullptr → process default).
/// @return            Reconstructed `mm × nn` image.
/// @see im2col
Value col2im(const Value &B, int m, int n, int mm, int nn,
             const std::string &block_type,
             std::pmr::memory_resource *mr = nullptr);

/// @brief Edge-preserving bilateral filter
/// (`B = imbilatfilt(I, degreeOfSmoothing, spatialSigma)`).
///
/// Weights combine a spatial Gaussian (σ = `spatialSigma`) and a
/// range Gaussian over intensity difference (variance =
/// `degreeOfSmoothing`). Replicate boundary.
///
/// @param I                  Input image.
/// @param degreeOfSmoothing  Intensity-range variance.
/// @param spatialSigma       Spatial-Gaussian σ in pixels.
/// @param mr                 Memory resource (nullptr → process default).
/// @return                   Filtered image, same class as `I`.
Value imbilatfilt(const Value &I, double degreeOfSmoothing, double spatialSigma,
                  std::pmr::memory_resource *mr = nullptr);

/// @brief Add synthetic noise to an image
/// (`J = imnoise(I, mode, p1, p2)`).
///
/// Modes:
///
///   | mode              | params                  | output                                  |
///   | ----------------- | ----------------------- | --------------------------------------- |
///   | `"gaussian"`      | m = 0, var = 0.01       | `J = I + m + sqrt(var)·N(0, 1)`         |
///   | `"localvar"`      | V (variance map)        | `J = I + sqrt(V)·N(0, 1)`               |
///   | `"salt & pepper"` | density d = 0.05        | fraction d set to {0, 1}                |
///   | `"speckle"`       | var = 0.04              | `J = I + I·N(0, var)`                   |
///   | `"poisson"`       | (no params)             | Poisson with mean = `I·scale`           |
///
/// Image is treated as unit-range [0, 1] internally; output is cast
/// back to the input class with saturation.
///
/// @param I    Input image.
/// @param mode One of the strings above.
/// @param p1   First parameter (see table).
/// @param p2   Second parameter (see table).
/// @param mr   Memory resource (nullptr → process default).
/// @return     Noisy image, same class as `I`.
Value imnoise(const Value &I, const std::string &mode,
              const Value &p1, const Value &p2,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Local sample standard deviation
/// (`B = stdfilt(I, domain)`).
///
/// Standard deviation of the neighbourhood defined by `domain`
/// (logical / numeric mask; non-zero entries select neighbours).
/// Default `domain = ones(3)`. Boundary = symmetric. Output is
/// double.
///
/// @param I       Input image.
/// @param domain  Neighbourhood mask (logical / numeric).
/// @param mr      Memory resource (nullptr → process default).
/// @return        DOUBLE matrix of local std-devs, same shape as `I`.
/// @see rangefilt, entropyfilt
Value stdfilt(const Value &I, const Value &domain,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Local max − min over a neighbourhood
/// (`B = rangefilt(I, domain)`).
///
/// Default `domain = true(3)`. Symmetric boundary. Output type
/// matches input class.
///
/// @param I       Input image.
/// @param domain  Neighbourhood mask.
/// @param mr      Memory resource (nullptr → process default).
/// @return        Local range image, same class as `I`.
/// @see stdfilt
Value rangefilt(const Value &I, const Value &domain,
                std::pmr::memory_resource *mr = nullptr);

/// @brief Octave-image style smoothing dispatcher
/// (`J = imsmooth(I, name, sigma)`).
///
/// Currently supports only `"Gaussian"`. 1-D-separable σ-Gaussian,
/// kernel half-width `ceil(3σ)`, default σ = 0.5, symmetric boundary,
/// computed in double then cast back to the input class. RGB inputs
/// are processed per-channel.
///
/// @param I      Input image.
/// @param name   Smoothing kernel name (`"Gaussian"`).
/// @param sigma  Gaussian σ. Default 0.5.
/// @param mr     Memory resource (nullptr → process default).
/// @return       Smoothed image, same class as `I`.
/// @throws Error Unknown smoothing name.
Value imsmooth(const Value &I, const std::string &name, double sigma,
               std::pmr::memory_resource *mr = nullptr);

/// @brief 3-D box (mean) filter
/// (`J = imboxfilt3(V, fH, fW, fP)`).
///
/// Per-axis filter sizes default to 3. Replicate boundary on all 3
/// axes. Output type matches input.
///
/// @param V   Input volume (3-D).
/// @param fH  Filter height (rows).
/// @param fW  Filter width (cols).
/// @param fP  Filter depth (pages).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Filtered volume, same class as `V`.
/// @see imboxfilt, imgaussfilt3
Value imboxfilt3(const Value &V, int fH, int fW, int fP,
                 std::pmr::memory_resource *mr = nullptr);

/// @brief 2-D convolution matrix (`T = convmtx2(h, m, n)`).
///
/// Builds the dense matrix `T` for 2-D 'full' convolution. Shape:
/// `(m+M-1)·(n+N-1) × m·n` where `h` is `M × N`. Multiplying T by
/// `vec(I)` (column-major) produces `vec(conv2(I, h, 'full'))`.
/// The result is returned dense (numkit has no sparse storage
/// yet). Output type is double regardless of input class.
///
/// @param h   Convolution kernel (`M × N`).
/// @param m   Image row count.
/// @param n   Image column count.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Dense convolution matrix, DOUBLE.
Value convmtx2(const Value &h, int m, int n,
               std::pmr::memory_resource *mr = nullptr);

/// @brief 3-D Gaussian filter
/// (`J = imgaussfilt3(V, sigH, sigW, sigP)`).
///
/// Separable 1-D convolutions along each axis with replicate
/// boundary. Per-axis filter sizes default to `2·ceil(2σ)+1`.
/// Output type matches input.
///
/// @param V     Input volume (3-D).
/// @param sigH  σ along rows (pixels).
/// @param sigW  σ along cols (pixels).
/// @param sigP  σ along pages (pixels).
/// @param mr    Memory resource (nullptr → process default).
/// @return      Filtered volume, same class as `V`.
/// @see imgaussfilt
Value imgaussfilt3(const Value &V, double sigH, double sigW, double sigP,
                   std::pmr::memory_resource *mr = nullptr);

/// @brief 3-D median filter (`J = medfilt3(V, [M N P])`).
///
/// Default 3×3×3 neighbourhood; sizes must be odd positive. Boundary
/// is symmetric (mirror reflection) by default. Output
/// class matches input. NB: only `'symmetric'` padopt is supported
/// in this first cut.
///
/// @param V   Input volume (3-D).
/// @param M   Neighbourhood rows.
/// @param N   Neighbourhood cols.
/// @param P   Neighbourhood pages.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Filtered volume, same class as `V`.
/// @see medfilt2
Value medfilt3(const Value &V, int M, int N, int P,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Local Shannon entropy (`B = entropyfilt(I, domain)`).
///
/// Computed in bits. 256-bin histogram for non-logical inputs
/// (uint8 / im2uint8 cast), 2 bins for logical. Default
/// `domain = ones(9)`. Symmetric boundary.
///
/// @param I       Input image.
/// @param domain  Neighbourhood mask.
/// @param mr      Memory resource (nullptr → process default).
/// @return        DOUBLE entropy image, same shape as `I`.
/// @see stdfilt, rangefilt
Value entropyfilt(const Value &I, const Value &domain,
                  std::pmr::memory_resource *mr = nullptr);

/// @brief 2-D order-statistic filter
/// (`B = ordfilt2(A, nth, domain, S)`).
///
/// For each output pixel the neighbourhood selected by `domain`
/// (non-zero entries) is gathered, optionally offset by `S` (same
/// size as domain), sorted, and the `nth`-order element returned
/// (1-based). `boundary` controls padding when the neighbourhood
/// crosses the image edge. Output class matches `A`.
///
/// @param A         Input image.
/// @param nth       Order-statistic index (1-based).
/// @param domain    Neighbourhood mask.
/// @param S         Offset matrix, same size as `domain`.
/// @param boundary  Padding mode for off-edge accesses.
/// @param pad_value Constant pad value (if `boundary == Constant`).
/// @param mr        Memory resource (nullptr → process default).
/// @return          Filtered image, same class as `A`.
/// @see medfilt2
Value ordfilt2(const Value &A, int nth, const Value &domain, const Value &S,
               PadMode boundary, double pad_value,
               std::pmr::memory_resource *mr = nullptr);

/// @brief 2-D frequency response of a filter
/// (`[H, f1, f2] = freqz2(h, M, N)`).
///
/// Evaluates `h` on a freqspace-style `M × N` grid (default 64×64).
/// `H[i, j] = Σ_{p, q} h[p, q]·exp(−iπ(f1[i]·p + f2[j]·q))`
/// where `f1` / `f2` are the row/column frequency vectors from
/// the `freqspace` grid.
///
/// @param h   Filter kernel.
/// @param M   Number of frequency samples along rows.
/// @param N   Number of frequency samples along cols.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Tuple `(H, f1, f2)`.
std::tuple<Value, Value, Value>
freqz2(const Value &h, size_t M, size_t N,
       std::pmr::memory_resource *mr = nullptr);

/// @brief Adaptive Wiener noise reduction
/// (`[J, noise] = wiener2(I, nh, nw, noise)`).
///
/// Lim 1989 eqs. 9.26-9.29. Local mean μ and variance σ² in an
/// `nh × nw` box neighbourhood (default 3×3, zero-pad boundary).
/// When `noise` is NaN it is estimated as the mean of σ² across the
/// image. Returns `(denoised, noise)` where `denoised` is cast back
/// to the input class.
///
/// @param I      Input image.
/// @param nh     Neighbourhood rows.
/// @param nw     Neighbourhood cols.
/// @param noise  Noise power estimate (NaN → measure from `I`).
/// @param mr     Memory resource (nullptr → process default).
/// @return       Tuple `(denoised, noise)`.
std::tuple<Value, Value>
wiener2(const Value &I, size_t nh, size_t nw, double noise,
        std::pmr::memory_resource *mr = nullptr);

/// @brief General sliding-neighbourhood operation
/// (`B = nlfilter(A, [m n], fun)`).
///
/// For every pixel in `A` extracts the `m × n` window centred on
/// that pixel (top-left bias for even sizes: pad rows above =
/// `floor((m-1)/2)`, below = `ceil((m-1)/2)`; similarly cols) and
/// passes it to `fun`. The result of `fun(window)` (a scalar) is
/// written to `B(i, j)`. Output class is the class of the FIRST
/// `fun` invocation's return value, matching MATLAB's behaviour.
///
/// **Padding.** Default `padval = 0`. The `'indexed'` form uses
/// `padval = 1` for `single` / `double` `A`, otherwise `padval = 0`.
///
/// @param eng         Engine used to dispatch `fun`.
/// @param A           Input image.
/// @param m           Neighbourhood row count.
/// @param n           Neighbourhood column count.
/// @param fun         Function-handle Value invoked as `fun(window)`.
/// @param indexed     `true` → 'indexed' padding rule (see above).
/// @param mr          Memory resource (nullptr → process default).
/// @return            Filtered image, same H×W as `A`, class = output
///                    class of `fun`.
Value nlfilter(numkit::Engine &eng, const Value &A,
               std::size_t m, std::size_t n, const Value &fun,
               bool indexed,
               std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::image
