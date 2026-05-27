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

/// @brief Edge-preserving guided filter (`B = imguidedfilter(A, G, ...)`).
///
/// Smooths image `A` using `G` as a guidance image (often `A`
/// itself for edge-preserving denoising). Outputs a value whose
/// local linear regression on `G` (within a neighborhood) best
/// reconstructs `A`:
///
///   meanI  = box(G)
///   meanP  = box(A)
///   corrI  = box(G·G)
///   corrIP = box(G·A)
///   varI  = corrI − meanI²
///   covIP = corrIP − meanI·meanP
///   a = covIP / (varI + ε)
///   b = meanP − a·meanI
///   B = box(a)·G + box(b)
///
/// Reference: K. He, J. Sun, X. Tang,
///   "Guided Image Filtering",
///   IEEE TPAMI 35(6), 1397-1409, 2013.
///
/// **Grayscale guide only** here. RGB-guide (color-covariance 3×3
/// inversion via Cramer's rule) and the Fast-Guided-Filter
/// downsample variant are deferred.
///
/// Default `nhood = 5` (square 5×5). Default `eps = 0.01 · diff(
/// getrangefromclass(A))²` (e.g. 0.01 for double, 650.25 for uint8).
///
/// Output class equals input class of `A`.
///
/// @param A      Image to filter (2-D; multi-channel uses same G per channel).
/// @param G      Guidance image (`Value::Empty` → reuse `A`). Same H×W as A.
/// @param nhood  Neighborhood side length (positive odd; default 5).
/// @param eps    Smoothing degree (negative → use class-based default).
/// @param mr     Memory resource (nullptr → process default).
/// @return       Filtered image, same shape and class as `A`.
Value imguidedfilter(const Value &A, const Value &G, int nhood,
                     double eps,
                     std::pmr::memory_resource *mr = nullptr);

/// @brief Anisotropic-diffusion edge-preserving smoothing
/// (`B = imdiffusefilt(I, ...)`).
///
/// Perona-Malik nonlinear diffusion (1990):
///   each iteration, compute nearest-neighbour intensity diffs,
///   weight them by a contrast-sensitive conduction coefficient,
///   then update I += rate · sum(flux).
///
/// Conductance:
///   exponential: c(∇) = exp(-(|∇|/K)²)
///   quadratic:   c(∇) = 1 / (1 + (|∇|/K)²)
///
/// `connectivity = "maximal"` (default) uses 8 neighbour
/// directions; `"minimal"` uses 4. Default `K = 0.1 · diff(
/// getrangefromclass(I))` (0.1 for double, 25.5 for uint8, etc.).
/// Default iterations N = 5 when `gradientThreshold` is scalar;
/// when it's a vector, `N = numel(gradientThreshold)` and each
/// iteration uses its own K.
///
/// 2-D inputs only (3-D / volume diffusion deferred — MATLAB's
/// `diffusefilt3D` adds 27-point stencils which roughly triples
/// the code).
///
/// References:
///   [1] P. Perona & J. Malik, "Scale-Space and Edge Detection
///       Using Anisotropic Diffusion", IEEE TPAMI 12(7), 1990.
///   [2] G. Gerig et al., "Nonlinear anisotropic filtering of MRI
///       data", IEEE Trans. Medical Imaging 11(2), 1992.
///
/// Computation uses double precision for double input and single
/// for everything else (matching MATLAB). Output class equals
/// input class.
///
/// @param I               2-D image (any real numeric class).
/// @param thresh          GradientThreshold (scalar or length-N
///                        vector); negative → use class default.
/// @param N               NumberOfIterations (0 → derive from
///                        thresh: 5 if scalar, length if vector).
/// @param connectivity    `"maximal"` (default) or `"minimal"`.
/// @param conduction      `"exponential"` (default) or `"quadratic"`.
/// @param mr              Memory resource (nullptr → process default).
/// @return                Smoothed image, same shape and class as `I`.
Value imdiffusefilt(const Value &I,
                    const Value &thresh,
                    std::size_t N,
                    const std::string &connectivity,
                    const std::string &conduction,
                    std::pmr::memory_resource *mr = nullptr);

/// @brief Apply a single Gabor filter to a 2-D image
/// (`[mag, phase] = imgaborfilt(A, wavelength, orientation, ...)`).
///
/// Single-filter form (the `gabor` MATLAB-OOP bank object is out
/// of scope per §0; multiple wavelengths/orientations require
/// calling this function in a loop).
///
/// Frequency-domain implementation matching MATLAB R2025b's
/// `images.internal.gaborFilterFFT`:
///
///   SigmaX = wavelength / π · √(log 2 / 2) · (2^B + 1) / (2^B − 1)
///   SigmaY = SigmaX / aspect
///   r      = max(⌈7·SigmaX⌉, ⌈7·SigmaY⌉)
///   Pad A by `r` replicate, FFT-2D, multiply by ifftshift(H),
///   IFFT-2D, crop back to size(A).
///
///   H(u, v) = 2π·SigmaX·SigmaY · exp(−½ ((u'−1/λ)²/σ_u² + v'²/σ_v²))
///       u', v' rotated by `orientation`, σ_u = 1/(2π·SigmaX),
///       σ_v = 1/(2π·SigmaY), u, v on a normalised frequency grid.
///
/// References:
///   [1] Jain & Farrokhnia, "Unsupervised Texture Segmentation
///       Using Gabor Filters", 1991.
///   [2] Kruizinga & Petkov, "Nonlinear Operator in Oriented
///       Texture", IEEE Trans. Image Processing 1999.
///
/// Output class equals input class (double or single).
///
/// @param A              2-D image (real, finite).
/// @param wavelength     Sinusoid wavelength (pixels/cycle), ≥ 2.
/// @param orientation    Filter orientation in degrees.
/// @param sfb            SpatialFrequencyBandwidth (default 1).
/// @param aspect         SpatialAspectRatio (default 0.5).
/// @param[out] mag_out   Magnitude response.
/// @param[out] phase_out Phase response (radians).
/// @param mr             Memory resource (nullptr → process default).
void imgaborfilt(const Value &A, double wavelength, double orientation,
                 double sfb, double aspect,
                 Value &mag_out, Value &phase_out,
                 std::pmr::memory_resource *mr = nullptr);

/// @brief Non-local means denoising
/// (`[J, estDoS] = imnlmfilt(I, ...)`).
///
/// Buades–Coll–Morel (2005) NLM filter — replace each pixel with
/// a weighted average of similar-patch pixels within a search
/// window. MATLAB uses the box-blur patch-distance variant
/// (mean squared pixel diff over the comparison window), not the
/// Gaussian-weighted original.
///
///   for each pixel p:
///     for each q in S×S search window around p:
///       d² = (1/|N|) Σ_{x∈N} (I(p+x) − I(q+x))²
///       w(p, q) = exp(−d² / h²)
///     J(p) = Σ_q w·I(q) / Σ_q w
///
/// The centre weight w(p, p) is set to the maximum non-self weight
/// (Buades' "max-trick") to avoid the self-similarity blowup.
///
/// Default `DegreeOfSmoothing` h is the Immerkaer-1996 noise
/// estimate:
///   |Laplacian(I)| · √(π/2) / (6·(W−2)·(H−2))
///
/// Reference:
///   [1] A. Buades, B. Coll, J.-M. Morel,
///       "A Non-Local Algorithm for Image Denoising", CVPR 2005.
///   [2] J. Immerkaer, "Fast Noise Variance Estimation",
///       CVIU 64(2), 1996.
///
/// Grayscale 2-D inputs only here; the RGB / colour path
/// (Euclidean distance summed across channels) is deferred.
///
/// Image must be at least 21×21 (MATLAB's hard minimum).
/// `SearchWindowSize` and `ComparisonWindowSize` must be odd
/// positive integers; `CWS ≤ SWS ≤ min(H, W)`.
///
/// Output class equals input class.
///
/// @param I               2-D grayscale image.
/// @param dos             DegreeOfSmoothing h (negative → use
///                        Immerkaer estimate).
/// @param swsize          SearchWindowSize (default 21).
/// @param cwsize          ComparisonWindowSize (default 5).
/// @param[out] J_out      Denoised image.
/// @param[out] est_out    Effective `h` used (estimated if input
///                        was negative).
/// @param mr              Memory resource (nullptr → process default).
void imnlmfilt(const Value &I, double dos,
               int swsize, int cwsize,
               Value &J_out, double &est_out,
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

/// @brief Column-wise neighbourhood operation
/// (`B = colfilt(A, [m n], block_type, fun)`).
///
/// Rearranges either every sliding `m × n` window (`block_type =
/// "sliding"`) or every distinct (non-overlapping) `m × n` block
/// (`block_type = "distinct"`) of `A` into columns of a temporary
/// matrix, calls `fun` on that matrix, then rearranges the result
/// back. Faster than @ref nlfilter for batch-compatible kernels
/// since `fun` is invoked once on a wide matrix.
///
/// **Sliding mode.** `A` is zero-padded (or one-padded under
/// `indexed = true` for `double`/`single`). The matrix `X` passed
/// to `fun` is `m·n × (rows·cols)` (one column per centre pixel).
/// `fun(X)` must return a `1 × (rows·cols)` row vector, reshaped
/// back into the original `A`'s shape.
///
/// **Distinct mode.** `A` is zero-padded (or one-padded under
/// `indexed = true`) up to the next multiple of `[m n]`. `X` is
/// `m·n × (nb_rows·nb_cols)` (one column per distinct block).
/// `fun(X)` must return a matrix of the SAME shape; the columns
/// are unpacked back into blocks, then the padded image is cropped
/// to the original `size(A)`.
///
/// The optional `[mblock nblock]` MATLAB argument is a memory-only
/// optimisation (the docs note it does not change the result);
/// the engine adapter accepts it but ignores it.
///
/// Output class equals the class of `fun()`'s return value
/// (matches MATLAB R2025b).
///
/// @param eng         Engine used to dispatch `fun`.
/// @param A           Input image.
/// @param m           Block rows.
/// @param n           Block cols.
/// @param block_type  `"sliding"` or `"distinct"` (case-insensitive,
///                    abbreviated by leading character).
/// @param fun         Function-handle Value.
/// @param indexed     `true` ⇒ `'indexed'` padding (1 for floats, 0 otherwise).
/// @param mr          Memory resource (nullptr → process default).
/// @return            Filtered image (same H × W as `A` in sliding;
///                    same H × W as `A` in distinct, after cropping).
Value colfilt(numkit::Engine &eng, const Value &A,
              std::size_t m, std::size_t n,
              const std::string &block_type, const Value &fun,
              bool indexed,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Edge-aware fast local Laplacian filtering
/// (`B = locallapfilt(I, sigma, alpha, beta, ...)`).
///
/// Multi-scale image enhancement via per-pixel manipulation of the
/// Laplacian pyramid coefficients. Operating points:
///
///   * `alpha > 1` → smoothing / detail suppression.
///   * `alpha < 1` → detail enhancement.
///   * `alpha = 1` → no detail change (then `beta` may still rescale).
///   * `beta < 1`  → dynamic-range compression.
///   * `beta > 1`  → dynamic-range expansion.
///   * `beta = 1`  → preserve range.
///   * `sigma`     → magnitude of edges to consider. Larger = more
///                   coefficients treated as "details" (subject to
///                   `alpha`); smaller = more treated as "edges"
///                   (subject to `beta`).
///
/// Algorithm (Paris-Hasinoff-Kautz 2011, "Local Laplacian Filters",
/// SIGGRAPH 2011, Eq. 4-7; fast-LLF accel by Aubry-Paris-Hasinoff-
/// Kautz-Durand 2014, "Fast Local Laplacian Filters: Theory and
/// Applications", ACM TOG 33(5)):
///
///   1. Build Gaussian pyramid `G_I` of input `I`.
///   2. For each intensity sample `g_k ∈ {gmin, ..., gmax}`:
///        a. Remap `R_k = remap(I, g_k, sigma, alpha, beta)`.
///        b. Build Gaussian pyramid `G_{R_k}` of `R_k`.
///        c. Compute Laplacian `L_{R_k}[i] = G_{R_k}[i] -
///           upsample(G_{R_k}[i+1])`.
///        d. For each pyramid level i and pixel p, accumulate:
///             `L_out[i](p) += k_w(G_I[i](p), g_k) · L_{R_k}[i](p)`
///           where `k_w(v, g_k) = max(0, 1 - |v - g_k| / delta)` is
///           the triangular weight centred at `g_k`.
///   3. Top level `L_out[end] = G_I[end]`. Collapse the Laplacian
///      pyramid (`L_out[i] += upsample(L_out[i+1])`).
///
/// Remap function (per pixel):
///
///   `d = I - g`. If `|d| <= sigma`:
///     `I_new = g + sign(d) * sigma * (|d|/sigma)^alpha`  (detail).
///   Else (`|d| > sigma`):
///     `I_new = g + sign(d) * (beta * (|d| - sigma) + sigma)` (edge).
///
/// Pyramid filter is the 5×5 binomial separable kernel
/// `[1 4 6 4 1] / 16` (Burt-Adelson 1983, "The Laplacian Pyramid as
/// a Compact Image Code") with replicate boundary. Upsampling
/// inserts zero rows / cols then convolves with the same kernel
/// scaled by 4 to preserve the local mean.
///
/// **RGB input**: `ColorMode = "luminance"` (default) computes the
/// gray response `Y = 0.298936·R + 0.587043·G + 0.114021·B`, filters
/// `Y`, then rescales the original RGB by the per-pixel ratio
/// `Y_filtered / Y_orig`. `ColorMode = "separate"` filters each
/// channel independently.
///
/// **NumIntensityLevels**: `"auto"` picks `50` for `alpha < 0.1`,
/// linear ramp `round(((50·0.9 - 16·0.1) - (50-16)·alpha) / 0.8)`
/// for `0.1 ≤ alpha < 0.9`, and `16` otherwise. Higher = better
/// quality at the cost of more pyramid builds.
///
/// **Special cases**:
///   - `alpha == 1 && beta == 1` → passthrough.
///   - `sigma == 0 && beta == 1` → passthrough.
///   - Flat image (`min == max`) → passthrough.
///   - `numIntensityLevels == 1` → single remap at `(min+max)/2`.
///
/// **Output class**: matches input class. `uint8`/`uint16` use
/// saturating `im2*` conversions; `int8`/`int16` likewise; `single`
/// passes through with no clipping.
///
/// @param I                Grayscale `H × W` or RGB `H × W × 3` image
///                         (`single`/`uint8`/`uint16`/`int8`/`int16`).
/// @param sigma            Edge-amplitude threshold (`>= 0`).
/// @param alpha            Detail-curve exponent (`> 0`).
/// @param beta             Edge-curve slope (`>= 0`, default 1).
/// @param num_intensity_levels  Pyramid samples (`-1` for auto).
/// @param process_luminance  `true` = "luminance" mode for RGB,
///                         `false` = "separate" (per-channel).
/// @param mr               Memory resource (nullptr → process default).
/// @return                 Filtered image, same class and shape as `I`.
Value locallapfilt(const Value &I, double sigma, double alpha,
                   double beta, int num_intensity_levels,
                   bool process_luminance,
                   std::pmr::memory_resource *mr = nullptr);

/// @brief Dark-channel-prior haze removal
/// (`[J, T, L] = imreducehaze(I, amount, ...)`).
///
/// Single-image dehazing via the dark-channel-prior atmospheric
/// model (He-Sun-Tang 2011, "Single Image Haze Removal Using Dark
/// Channel Prior", IEEE TPAMI 33(12)). Two algorithm flavours:
///
///   * `"simpledcp"` (default) — per-pixel dark channel
///     `D(x) = min_c I_c(x)`; atmospheric light estimated by
///     5-level quadtree decomposition (Dubok et al. 2014, ICIP)
///     on the dark channel; transmission map refined via
///     guided filter with a 5×5 neighbourhood and ε=0.01;
///     ω=0.9 (mild residual haze).
///   * `"approxdcp"` — patched dark channel via min-erosion with
///     square strel of size `ceil(min(H,W)/400·15)`; atmospheric
///     light estimated from the 0.1% brightest dark-channel pixels;
///     transmission map computed via `1 - imopen(A/L, strel)` and
///     refined with guided filter (radius `ceil(min(H,W)/50)`,
///     ε=1e-4, subsample=min(4, radius)); ω=0.95.
///
/// Atmospheric-light estimation:
///   - When `atmospheric_light` is `Value::Empty` and method is
///     `"simpledcp"`: quadtree on `min_c I_c` eroded by the strel,
///     picking the brightest-mean quadrant for 5 levels, then the
///     pixel in that quadrant with minimum Euclidean distance to
///     `[1, 1, 1]`. For images smaller than 64×64 the quadtree is
///     skipped and the full image is searched.
///   - When `atmospheric_light` is `Value::Empty` and method is
///     `"approxdcp"`: take the 0.1% brightest pixels of the
///     dark channel; among those, pick the pixel with the highest
///     grayscale luminance.
///
/// Scene radiance: `R(x) = L + (I(x) - L) / max(t̃(x), t₀)` with
/// `t̃ = 1 - ω·(1-t)`, `t₀ = 0.1`. Final blend by user `amount`:
/// `t' = min(1, t̃ + amount)`, `J = R·t' + L·(1-t')`.
///
/// Post-processing (`contrast_enhancement`):
///   * `"global"` (default) — `A.^0.75`, `mat2gray`, `stretchlim
///     [0.001 0.999]`, `imadjust` with `clip + 0.8·(max(clip,mean)
///     - clip)`.
///   * `"boost"` — `J·(1 + amount·boost_amount·(1-T))`.
///   * `"none"`  — no post-processing.
///
/// Outputs:
///   - `J` — dehazed image, same class and shape as `I`. For
///     `uint8`/`uint16` returns saturated `im2*`; for `single` /
///     `double` returns clipped to `[0, 1]`.
///   - `T` — haze thickness (`1 - t`), DOUBLE H×W.
///   - `L` — estimated atmospheric light. DOUBLE 1×3 for RGB,
///     DOUBLE scalar for grayscale.
///
/// Returns only `J` (the typed entry-point packs `T` and `L` into
/// `t_out` / `L_out` references). When `amount == 0`, the function
/// short-circuits to return the input unchanged with empty `T` /
/// `L`.
///
/// @param I                   `H × W` (grayscale) or `H × W × 3`
///                            (RGB), `uint8`/`uint16`/`single`/
///                            `double`.
/// @param amount              ∈ [0, 1]. 1 = full removal.
/// @param method              `"simpledcp"` (def) / `"approxdcp"`.
/// @param atmospheric_light   Empty → estimate. Otherwise 1×3
///                            (RGB) or scalar (grayscale) DOUBLE in
///                            `[0, 1]`.
/// @param contrast_enhancement `"global"` (def) / `"boost"` / `"none"`.
/// @param boost_amount        ∈ [0, 1] for `"boost"`, default 0.1.
/// @param t_out               (output) Haze thickness, DOUBLE H×W.
/// @param L_out               (output) Atmospheric light, DOUBLE
///                            1×3 (RGB) or scalar (gray).
/// @param mr                  Memory resource (nullptr → default).
/// @return                    Dehazed image `J`.
Value imreducehaze(const Value &I, double amount,
                   const std::string &method,
                   const Value &atmospheric_light,
                   const std::string &contrast_enhancement,
                   double boost_amount,
                   Value &t_out, Value &L_out,
                   std::pmr::memory_resource *mr = nullptr);

/// @brief Multi-scale Frangi vesselness filter
/// (`J = fibermetric(I, thickness, ...)`).
///
/// Enhances elongated / tubular structures (vessels, fibres, filaments)
/// at user-specified scales. For each thickness `t_i`:
///
///   - σ_i = t_i / 6.
///   - Gaussian-smooth `I` with FilterSize = `2·ceil(3σ_i)+1`.
///   - Compute Hessian (3 elements for 2-D, 6 for 3-D) by central
///     finite differences.
///   - Compute Hessian eigenvalues.
///   - Apply Frangi 1998 vesselness:
///     * **2-D**: `λ₁, λ₂` with `|λ₁| ≤ |λ₂|`. Bright structures need
///       `λ₂ < 0`; dark need `λ₂ > 0` (else vesselness = 0).
///       `Rβ = λ₁/λ₂`,  `S = sqrt(λ₁² + λ₂²)`.
///       `V = exp(-Rβ²/(2·β²)) · (1 - exp(-S²/(2·c²)))`,
///       with `β = 0.5` (fixed).
///     * **3-D**: `λ₁, λ₂, λ₃` with `|λ₁| ≤ |λ₂| ≤ |λ₃|`. Bright
///       need `λ₂ < 0 && λ₃ < 0`; dark need both `> 0`.
///       `Rα = |λ₂|/|λ₃|` (plate-vs-line),
///       `Rβ = |λ₁|/sqrt(|λ₂·λ₃|)` (blobness),
///       `S² = λ₁² + λ₂² + λ₃²`.
///       `V = (1 - exp(-Rα²/(2α²))) · exp(-Rβ²/(2β²)) ·
///             (1 - exp(-S²/(2c²)))`,
///       with `α = β = 0.5` (fixed).
///
/// Per-pixel max over all `thickness` scales.
///
/// `c` (`structure_sensitivity`) defaults to
/// `diff(getrangefromclass(I)) / 100` (e.g. 2.55 for uint8,
/// 655.35 for uint16, 0.01 for single/double). Pass `-1.0` for the
/// class default.
///
/// `bright_polarity = true` (default) detects light-on-dark structures.
/// `false` detects dark-on-light.
///
/// **Input class**: `uint8`, `uint16`, `uint32`, `int8`, `int16`,
/// `int32`, `single`, `double` 2-D or 3-D image (all dimensions ≥ 2).
/// **Output class**: `single` for everything except `double` input,
/// which preserves `double`.
///
/// References:
///   Frangi, A. F., Niessen, W. J., Vincken, K. L., Viergever, M. A.
///   (1998). Multiscale vessel enhancement filtering. MICCAI 1998.
///
/// @param I                       Grayscale 2-D image or 3-D volume.
/// @param thickness               Vector of thickness scales (default
///                                = `[4 6 8 10 12 14]` if empty).
/// @param structure_sensitivity   `c` (`-1` = class default).
/// @param bright_polarity         `true` = bright (default).
/// @param mr                      Memory resource (nullptr → default).
/// @return                        Vesselness response, same shape as `I`.
Value fibermetric(const Value &I, const std::vector<double> &thickness,
                  double structure_sensitivity, bool bright_polarity,
                  std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::image
