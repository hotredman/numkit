/// @file geom.hpp
/// @ingroup group_image
// toolboxes/image/include/numkit/image/geom/geom.hpp
//
// Basic geometric transforms: resize / crop / rotate / translate.
// All preserve the input element type (uint8, double, …) and accept
// either H×W (grayscale) or H×W×C (multi-channel) arrays.

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>
#include <string>

namespace numkit::image {

/// @addtogroup group_image
/// @{


/// Resample an image to a new size by a uniform scale factor
/// (`B = imresize(A, scale, method)`).
///
/// @param A       Input image (H×W or H×W×C).
/// @param scale   Positive scale factor (> 1 enlarges, < 1 shrinks).
/// @param method  `"nearest"` or `"bilinear"` (default).
/// @param mr      Memory resource (nullptr → process default).
/// @return        Resampled image; element type preserved.
///
/// @see imcrop, imrotate
Value imresize(const Value &A, double scale, const std::string &method,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Resample an image to an explicit output size
/// (`B = imresize(A, [outH outW], method)`).
///
/// Same as the scale-factor overload but with explicit target rows /
/// cols.
///
/// @param A       Input image (H×W or H×W×C).
/// @param outH    Output row count.
/// @param outW    Output column count.
/// @param method  `"nearest"` or `"bilinear"` (default).
/// @param mr      Memory resource (nullptr → process default).
/// @return        Resampled image; element type preserved.
/// @see imcrop, imrotate
Value imresize(const Value &A, size_t outH, size_t outW,
               const std::string &method,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Crop a rectangular region
/// (`B = imcrop(A, [xmin ymin width height])`).
///
/// Coordinates are 1-based. Sub-pixel coordinates are
/// rounded; out-of-bounds requests are clamped to the image extent.
///
/// @param A       Input image.
/// @param xmin    Top-left column (1-based).
/// @param ymin    Top-left row (1-based).
/// @param width   Rectangle width (columns).
/// @param height  Rectangle height (rows).
/// @param mr      Memory resource (nullptr → process default).
/// @return        Cropped sub-image.
/// @see imresize
Value imcrop(const Value &A, double xmin, double ymin,
             double width, double height,
             std::pmr::memory_resource *mr = nullptr);

/// @brief Crop a 3-D image volume (`VOUT = imcrop3(V, cuboid)`).
///
/// `cuboid = [XMIN YMIN ZMIN WIDTH HEIGHT DEPTH]`. The crop volume
/// covers columns `round(XMIN) .. round(XMIN+WIDTH)`, rows
/// `round(YMIN) .. round(YMIN+HEIGHT)`, and pages
/// `round(ZMIN) .. round(ZMIN+DEPTH)` — i.e. an inclusive
/// `(width+1) × (height+1) × (depth+1)` block in MATLAB's spatial
/// (X, Y, Z) = (col, row, page) convention.
///
/// Class is preserved. Multi-channel 3-D / 4-D volumes are supported:
/// the 4th dimension passes through unchanged. Out-of-bounds cuboid
/// throws.
///
/// @param V       3-D / 4-D numeric, logical, or integer volume.
/// @param cuboid  6-element `[XMIN YMIN ZMIN W H D]` vector.
/// @param mr      Memory resource (nullptr → process default).
/// @return        Cropped sub-volume.
Value imcrop3(const Value &V, const Value &cuboid,
              std::pmr::memory_resource *mr = nullptr);

/// Rotate an image counter-clockwise (`B = imrotate(A, angle, method, bbox)`).
///
/// @param A       Input image.
/// @param angle   Rotation in degrees, CCW positive.
/// @param method  `"nearest"` or `"bilinear"` (default).
/// @param bbox    `"loose"` (default — expand to fit rotated extent)
///                or `"crop"` (keep input dims; clip rotated pixels
///                that fall outside).
/// @param mr      Memory resource (nullptr → process default).
/// @return        Rotated image; pixels outside source filled with 0.
Value imrotate(const Value &A, double angle, const std::string &method,
               const std::string &bbox,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Shift an image by an arbitrary `(dx, dy)` translation
/// (`B = imtranslate(A, [dx dy])`).
///
/// Half-pixel shifts use bilinear interpolation; integer shifts use
/// a fast nearest-neighbour copy. Same dims as input; out-of-source
/// pixels filled with 0.
///
/// @param A   Input image.
/// @param dx  Shift along columns (sub-pixel allowed).
/// @param dy  Shift along rows.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Translated image, same shape and class as `A`.
/// @see imrotate, imresize
Value imtranslate(const Value &A, double dx, double dy,
                  std::pmr::memory_resource *mr = nullptr);

/// World-axis ↔ pixel coordinate mapping (`pix = axes2pix(n, extent, axesCoord)`).
///
/// Maps user-axis coordinates back to 1-based intrinsic pixel
/// coordinates assuming the first / last pixel centres lie at
/// `extent(1)` / `extent(end)`. Degenerate cases (n = 1 or
/// `extent(1) == extent(end)`) collapse the mapping to a translation.
///
/// @param n         Pixel count along the axis.
/// @param extent    Two-element vector [low, high] of axis bounds.
/// @param axesCoord Array of axis-space coordinates to convert.
/// @param mr        Memory resource (nullptr → process default).
/// @return          Pixel-space coordinates, same shape as `axesCoord`.
Value axes2pix(double n, const Value &extent, const Value &axesCoord,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Resample a 3-D volume to a new size
/// (`B = imresize3(V, scale|[r c d], method, ...)`).
///
/// Separable 1-D resampling along rows / cols / planes using the
/// same kernel and boundary convention as MATLAB R2025b imresize:
/// 1-indexed centered coordinate `u = o/scale + 0.5·(1 − 1/scale)`,
/// mirror boundary `aux = [1..N, N..1]` of cycle `2N`,
/// `P = ⌈kernel_width⌉ + 2` taps per output sample, weights
/// normalized to sum to 1. With `antialias = true` and a shrink
/// (`scale < 1`) the kernel is stretched via
/// `h(x) = scale · k(scale · x)` (MATLAB's anti-aliasing).
///
/// References:
/// - Keys 1981 — Catmull-Rom cubic (default 'cubic', `a = −0.5`).
/// - Duchon 1979 — Lanczos windowed-sinc kernels.
/// - Smith 1995 ("A pixel is not a little square…") — pixel-center
///   coordinate convention.
///
/// @param V         3-D numeric volume.
/// @param outR      Output row count.
/// @param outC      Output column count.
/// @param outD      Output plane count.
/// @param method    `"nearest"`, `"box"`, `"triangle"` (= `"linear"`),
///                  `"cubic"` (default), `"lanczos2"`, or `"lanczos3"`.
/// @param antialias `true` to enable anti-aliasing when shrinking.
/// @param mr        Memory resource (nullptr → process default).
/// @return          Resampled volume; element type preserved with clip.
Value imresize3(const Value &V, size_t outR, size_t outC, size_t outD,
                const std::string &method, bool antialias,
                std::pmr::memory_resource *mr = nullptr);

/// @brief Resample a 3-D volume by a uniform scale factor
/// (`B = imresize3(V, scale, ...)`). See size-vector overload.
Value imresize3(const Value &V, double scale,
                const std::string &method, bool antialias,
                std::pmr::memory_resource *mr = nullptr);

/// @brief Rotate a 3-D volume around an arbitrary axis
/// (`B = imrotate3(V, angle, W, method, bbox, FillValues=v)`).
///
/// Rotates `V` by `angle` degrees counterclockwise (right-hand rule)
/// around the axis vector W = [Wx Wy Wz] passing through the centre
/// of the volume. Uses the Rodrigues rotation matrix and applies an
/// inverse-warp on each output voxel:
///
///     (xs, ys, zs) = inCentre + R · ((xo, yo, zo) − outCentre)
///
/// where the spatial X / Y / Z = col / row / page (MATLAB
/// convention). Out-of-bounds source samples evaluate to `fill`.
///
/// References:
/// - Rodrigues 1840 — axis-angle rotation matrix.
/// - Keys 1981 — Catmull-Rom tricubic kernel.
///
/// @param V          3-D numeric volume.
/// @param angle_deg  Rotation in degrees (CCW, right-hand rule).
/// @param Wx,Wy,Wz   Components of the axis vector (need not be unit).
/// @param method     `"nearest"`, `"linear"` (default), or `"cubic"`.
/// @param bbox       `"loose"` (default — expand to fit rotated extent)
///                   or `"crop"` (keep input dims).
/// @param fill       Out-of-bounds fill value (default 0).
/// @param mr         Memory resource (nullptr → process default).
/// @return           Rotated volume; element type preserved.
Value imrotate3(const Value &V, double angle_deg,
                double Wx, double Wy, double Wz,
                const std::string &method, const std::string &bbox,
                double fill,
                std::pmr::memory_resource *mr = nullptr);

/// Burt–Adelson image pyramid step (`B = impyramid(A, type)`).
///
/// Implements one level of the classic Burt–Adelson Gaussian pyramid
/// with a 5-tap separable kernel `[0.05 0.25 0.4 0.25 0.05]` and
/// replicate boundary handling. 3-D inputs are processed per-channel.
///
/// @param A     Input image.
/// @param type  `"reduce"` (output ceil(M/2)×ceil(N/2) after low-pass)
///              or `"expand"` (output (2M-1)×(2N-1) after zero-stuff).
/// @param mr    Memory resource (nullptr → process default).
/// @return      Downsampled / upsampled image.
Value impyramid(const Value &A, const std::string &type,
                std::pmr::memory_resource *mr = nullptr);


/// @}
} // namespace numkit::image
