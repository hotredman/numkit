// libs/image/include/numkit/image/morph/morph.hpp
//
// Morphological operations. Function-form only: `strel` returns a
// numeric structuring-element matrix (LOGICAL HxW), and the operators
// accept either that matrix or any HxW logical / numeric mask.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <string>
#include <vector>

namespace numkit::image {

/// Build a 2-D structuring element (`SE = strel(shape, params)`).
///
/// Supported shapes:
///   - `"square"`     — N×N square; `params = [N]`.
///   - `"rectangle"`  — `params = [m, n]`.
///   - `"diamond"`    — `params = [r]` (radius).
///   - `"disk"`       — `params = [r]` (decomposed approximation).
///   - `"line"`       — `params = [L, theta_deg]`.
///   - `"arbitrary"` / `""` — caller-provided mask in `arbitrary_nhood`.
///
/// @param shape            Shape name (see table).
/// @param params           Numeric parameters (see table).
/// @param arbitrary_nhood  Mask for `"arbitrary"` shape (pass empty otherwise).
/// @param mr               Memory resource (nullptr → process default).
/// @return                 LOGICAL matrix (1 = element in neighbourhood).
///
/// @see imerode, imdilate
Value strel(const std::string &shape, const std::vector<double> &params,
            const Value &arbitrary_nhood,
            std::pmr::memory_resource *mr = nullptr);

/// Grayscale erosion (`J = imerode(I, SE)`).
///
/// Each output pixel is the minimum of `I` over the SE-marked
/// neighbourhood. For LOGICAL input this reduces to the binary
/// definition: pixel = 1 iff every SE-marked pixel is 1.
///
/// @see imdilate, imopen
Value imerode(const Value &I, const Value &SE,
              std::pmr::memory_resource *mr = nullptr);

/// Grayscale dilation (`J = imdilate(I, SE)`).
///
/// Each output pixel is the maximum of `I` over the SE-marked
/// neighbourhood.
Value imdilate(const Value &I, const Value &SE,
               std::pmr::memory_resource *mr = nullptr);

/// Morphological opening (`J = imopen(I, SE) = imdilate(imerode(I, SE), SE)`).
Value imopen(const Value &I, const Value &SE,
             std::pmr::memory_resource *mr = nullptr);

/// Morphological closing (`J = imclose(I, SE) = imerode(imdilate(I, SE), SE)`).
Value imclose(const Value &I, const Value &SE,
              std::pmr::memory_resource *mr = nullptr);

/// Morphological reconstruction by dilation
/// (`J = imreconstruct(marker, mask, conn)`).
///
/// Iteratively dilates `marker` and caps it element-wise against
/// `mask` until stable:
///   @f$ J_{k+1} = \min(\text{imdilate}(J_k, \text{SE}),\ \text{mask}) @f$,
///   @f$ J_0 = \text{marker} @f$.
///
/// Works on binary or grayscale inputs. `marker ≤ mask` element-wise
/// is required (we clip to the mask up-front to enforce it).
///
/// @param conn  Connectivity — 4 or 8 (default 8).
/// @return      Reconstructed image, same H×W as input.
///
/// @see imfill_holes, imhmax, imclearborder
Value imreconstruct(const Value &marker, const Value &mask, int conn,
                    std::pmr::memory_resource *mr = nullptr);

/// Fill holes in a binary image (`J = imfill(BW, 'holes', conn)`).
///
/// Holes are 0-pixels NOT connectivity-reachable from the image
/// border. Implementation:
///   `marker = ~BW restricted to the image border`,
///   `R      = imreconstruct(marker, ~BW, conn)`,
///   `J      = ~R`.
Value imfill_holes(const Value &BW, int conn,
                   std::pmr::memory_resource *mr = nullptr);

/// Regional maxima mask (`J = imregionalmax(I, conn)`).
///
/// Connected pixel groups whose neighbours are all strictly smaller.
/// Standard formula:
/// @f$ \text{regmax}(I) = (I - \text{imreconstruct}(I - 1, I)) > 0 @f$.
Value imregionalmax(const Value &I, int conn,
                    std::pmr::memory_resource *mr = nullptr);

/// Regional minima mask (`J = imregionalmin(I, conn)`).
///
/// Dual of @ref imregionalmax via image inversion (`typeMax − I` for
/// unsigned, `−I` for floats).
Value imregionalmin(const Value &I, int conn,
                    std::pmr::memory_resource *mr = nullptr);

/// H-maxima transform (`J = imhmax(I, h, conn)`).
///
/// Suppresses regional maxima shallower than `h`:
/// @f$ \text{imhmax}(I, h) = \text{imreconstruct}(I - h, I, \text{conn}) @f$.
///
/// @see imregionalmax, imextendedmax
Value imhmax(const Value &I, double h, int conn,
             std::pmr::memory_resource *mr = nullptr);

/// H-minima transform (`J = imhmin(I, h, conn)`) — dual of @ref imhmax.
Value imhmin(const Value &I, double h, int conn,
             std::pmr::memory_resource *mr = nullptr);

/// Extended-maxima transform (`J = imextendedmax(I, h, conn)`).
///
/// Logical mask of regional maxima of @ref imhmax: keeps only peaks
/// with height ≥ `h` above their surroundings.
///   @f$ \text{imextendedmax}(I, h) = \text{imregionalmax}(\text{imhmax}(I, h)) @f$.
Value imextendedmax(const Value &I, double h, int conn,
                    std::pmr::memory_resource *mr = nullptr);

/// Extended-minima transform (`J = imextendedmin(I, h, conn)`).
///
/// Dual of @ref imextendedmax: troughs with depth ≥ `h`.
Value imextendedmin(const Value &I, double h, int conn,
                    std::pmr::memory_resource *mr = nullptr);

/// Minima imposition (`J = imimposemin(I, BW, conn)`).
///
/// Modifies grayscale `I` so its only regional minima sit at the
/// marker pixels in `BW`. Soille's reconstruction-by-erosion recipe:
///   `marker fm = -∞ at BW, +∞ elsewhere`,
///   `mask    m = min(I, fm) = -∞ at BW, I elsewhere`,
///   `J         = R^E_m(fm)` (fill non-marker basins up to their
///                            boundary, keep markers at the global floor).
///
/// Useful as the pre-processing step for marker-controlled watershed.
Value imimposemin(const Value &I, const Value &BW, int conn,
                  std::pmr::memory_resource *mr = nullptr);

/// Remove border-touching components (`J = imclearborder(BW, conn)`).
///
/// Standard reconstruction recipe:
///   `marker = BW restricted to the rim`,
///   `R      = imreconstruct(marker, BW, conn)`,
///   `J      = BW & ~R`.
Value imclearborder(const Value &BW, int conn,
                    std::pmr::memory_resource *mr = nullptr);

/// Keep only border-touching components (`J = imkeepborder(BW, conn)`).
///
/// Exact dual of @ref imclearborder:
///   `marker = BW restricted to the rim`,
///   `J      = imreconstruct(marker, BW, conn)`.
Value imkeepborder(const Value &BW, int conn,
                   std::pmr::memory_resource *mr = nullptr);

/// White top-hat (`J = imtophat(I, SE) = I − imopen(I, SE)`).
///
/// Highlights bright structures smaller than `SE` on a dark
/// background. Output saturates to the input class.
Value imtophat(const Value &I, const Value &SE,
               std::pmr::memory_resource *mr = nullptr);

/// Black top-hat (`J = imbothat(I, SE) = imclose(I, SE) − I`).
///
/// Highlights dark structures smaller than `SE` on a bright
/// background. Output saturates to the input class.
Value imbothat(const Value &I, const Value &SE,
               std::pmr::memory_resource *mr = nullptr);

/// Morphological gradient (`G = mmgradm(I, se_dil, se_ero)`).
///
/// Default: @f$ G = \text{imdilate}(I) - \text{imerode}(I) @f$ with
/// elementary cross (diamond, radius 1) structuring element. For
/// logical inputs the subtraction collapses to `dilate & ~erode`.
///
/// Half-gradients:
///   - `mmgradm(I, Value::Empty, se_ero)` → −imerode (internal).
///   - `mmgradm(I, se_dil, Value::Empty)` → imdilate (external).
Value mmgradm(const Value &I, const Value &se_dil, const Value &se_ero,
              std::pmr::memory_resource *mr = nullptr);

/// Pack a binary image into uint32 columns (`BWP = bwpack(BW)`).
///
/// Each 32 rows of `BW` become one uint32; bit 0 corresponds to the
/// first row, bit 31 to the 32nd. Output rows = `ceil(M / 32)`.
/// Zero-padded if M is not a multiple of 32.
///
/// @see bwunpack, bwmorph (faster on packed inputs)
Value bwpack(const Value &BW,
             std::pmr::memory_resource *mr = nullptr);

/// Inverse of @ref bwpack (`BW = bwunpack(BWP, M)`).
///
/// `M` is the original row count; defaults to `rows(BWP) * 32`.
/// Output is LOGICAL.
Value bwunpack(const Value &BWP, size_t M,
               std::pmr::memory_resource *mr = nullptr);

/// Neighbourhood-LUT filter (`A = applylut(BW, LUT)`).
///
/// LUT length must be `2^(n²)` for some n; common choices are n = 2
/// (length 16) and n = 3 (length 512). Each output pixel is the LUT
/// entry indexed by the bit-pattern of its `n × n` neighbourhood
/// (zero-padded), with kernel weights `reshape(2^[nq-1:-1:0], n, n)`.
Value applylut(const Value &BW, const Value &LUT,
               std::pmr::memory_resource *mr = nullptr);

/// Binary hit-or-miss transform (`J = bwhitmiss(BW, se1, se2)`).
///
///   @f$ J = \text{imerode}(BW, se_1)\ \wedge\ \text{imerode}(\sim BW, se_2) @f$.
///
/// `se1` marks foreground requirements; `se2` marks background
/// requirements. They must have disjoint neighbourhoods to be
/// meaningful (not enforced).
Value bwhitmiss(const Value &BW, const Value &se1, const Value &se2,
                std::pmr::memory_resource *mr = nullptr);

/// Composite binary morphological operations (`J = bwmorph(BW, op, n)`).
///
/// Faithful port of MATLAB R2025b's `bwmorph`. Each named operation
/// corresponds to a 512-entry LUT (or a chain of LUTs and bitwise
/// compositions), indexed by the 3×3 neighbourhood of every pixel.
/// Out-of-bounds pixels are 0.
///
/// Supported operations (case-sensitive — caller normalises):
///   - single-LUT: `"bridge" "clean" "diag" "dilate" "endpoints"`
///     `"erode" "fatten" "fill" "hbreak" "majority" "perim4" "perim8"`
///     `"remove"`.
///   - composite chains: `"bothat" "close" "open" "tophat" "shrink"`
///     `"skeleton" "spur" "thin" "thicken" "branchpoints"`.
///
/// @param BW  Binary input.
/// @param op  Operation name.
/// @param n   Iteration count. `n = -1` means "until stable"
///            (MATLAB's `Inf`); `n = 1` is the default.
/// @param mr  Memory resource (nullptr → process default).
Value bwmorph(const Value &BW, const std::string &op, int n,
              std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::image
