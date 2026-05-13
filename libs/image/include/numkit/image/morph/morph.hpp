// libs/image/include/numkit/image/morph/morph.hpp
//
// Morphological operations.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <string>
#include <vector>

namespace numkit::image {

/// @file
/// @brief Morphological operations (function-form).
///
/// **Inputs.** Most operators accept an `H × W` (or `H × W × P`) image
/// `I` plus a structuring element `SE` produced by @ref strel (LOGICAL
/// matrix). LOGICAL inputs use binary morphology; numeric inputs use
/// grayscale min/max formulations.
///
/// **`conn`.** Connectivity argument is 4 or 8 for 2-D; default 8.

/// @brief Build a 2-D structuring element
/// (`SE = strel(shape, params, arbitrary_nhood)`).
///
/// Supported shapes:
/// - `"square"`     — N×N square; `params = [N]`.
/// - `"rectangle"`  — `params = [m, n]`.
/// - `"diamond"`    — `params = [r]` (radius).
/// - `"disk"`       — `params = [r]` (decomposed approximation).
/// - `"line"`       — `params = [L, theta_deg]`.
/// - `"arbitrary"` / `""` — caller-provided mask in `arbitrary_nhood`.
///
/// @param shape            Shape name (see table).
/// @param params           Numeric shape parameters.
/// @param arbitrary_nhood  Mask for `"arbitrary"` (empty otherwise).
/// @param mr               Memory resource (nullptr → process default).
/// @return                 LOGICAL matrix (1 = neighbourhood pixel).
/// @throws Error           Unknown shape (`m:strel:badShape`).
/// @see imerode, imdilate
Value strel(const std::string &shape, const std::vector<double> &params,
            const Value &arbitrary_nhood,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Grayscale erosion (`J = imerode(I, SE)`).
///
/// Each output pixel is the minimum of `I` over the SE-marked
/// neighbourhood. For LOGICAL input: pixel = 1 iff every SE-marked
/// neighbour is 1.
///
/// @param I   Input image.
/// @param SE  Structuring element.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Eroded image, same shape as `I`.
/// @see imdilate, imopen, strel
Value imerode(const Value &I, const Value &SE,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Grayscale dilation (`J = imdilate(I, SE)`).
///
/// Each output pixel is the maximum of `I` over the SE-marked
/// neighbourhood.
///
/// @param I   Input image.
/// @param SE  Structuring element.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Dilated image.
/// @see imerode, imclose
Value imdilate(const Value &I, const Value &SE,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Morphological opening
/// (`J = imopen(I, SE) = imdilate(imerode(I, SE), SE)`).
/// @param I   Input image. @param SE  Structuring element.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Opened image. @see imclose
Value imopen(const Value &I, const Value &SE,
             std::pmr::memory_resource *mr = nullptr);

/// @brief Morphological closing
/// (`J = imclose(I, SE) = imerode(imdilate(I, SE), SE)`).
/// @param I   Input image. @param SE  Structuring element.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Closed image. @see imopen
Value imclose(const Value &I, const Value &SE,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Morphological reconstruction by dilation
/// (`J = imreconstruct(marker, mask, conn)`).
///
/// Iteratively dilates `marker` and caps element-wise against `mask`
/// until stable:
/// @f$ J_{k+1} = \min(\text{imdilate}(J_k, SE),\ \text{mask}) @f$,
/// @f$ J_0 = \text{marker} @f$.
/// Works on binary or grayscale inputs. `marker <= mask` element-wise
/// is required (we clip up-front to enforce).
///
/// @param marker  Seed image.
/// @param mask    Upper-bound image (`marker <= mask`).
/// @param conn    Connectivity (4 or 8).
/// @param mr      Memory resource (nullptr → process default).
/// @return        Reconstructed image, same shape as `marker`.
/// @see imfill_holes, imhmax, imclearborder
Value imreconstruct(const Value &marker, const Value &mask, int conn,
                    std::pmr::memory_resource *mr = nullptr);

/// @brief Fill holes in a binary image (`J = imfill(BW, 'holes', conn)`).
///
/// Holes are 0-pixels NOT connectivity-reachable from the image
/// border. Implementation: reconstruct `~BW` from its border seed,
/// then invert.
///
/// @param BW    Binary image.
/// @param conn  Connectivity (4 or 8).
/// @param mr    Memory resource (nullptr → process default).
/// @return      Hole-filled binary image.
/// @see imreconstruct
Value imfill_holes(const Value &BW, int conn,
                   std::pmr::memory_resource *mr = nullptr);

/// @brief Regional maxima mask (`J = imregionalmax(I, conn)`).
///
/// Connected pixel groups whose neighbours are all strictly smaller.
/// Standard formula:
/// @f$ \text{regmax}(I) = (I - \text{imreconstruct}(I - 1, I)) > 0 @f$.
///
/// @param I     Input image.
/// @param conn  Connectivity.
/// @param mr    Memory resource (nullptr → process default).
/// @return      LOGICAL maxima mask, same shape as `I`.
/// @see imregionalmin, imextendedmax
Value imregionalmax(const Value &I, int conn,
                    std::pmr::memory_resource *mr = nullptr);

/// @brief Regional minima mask (`J = imregionalmin(I, conn)`).
///
/// Dual of @ref imregionalmax via image inversion (`typeMax - I` for
/// unsigned, `-I` for floats).
///
/// @param I     Input image.
/// @param conn  Connectivity.
/// @param mr    Memory resource (nullptr → process default).
/// @return      LOGICAL minima mask.
/// @see imregionalmax
Value imregionalmin(const Value &I, int conn,
                    std::pmr::memory_resource *mr = nullptr);

/// @brief H-maxima transform (`J = imhmax(I, h, conn)`).
///
/// Suppresses regional maxima shallower than `h`:
/// @f$ \text{imhmax}(I, h) = \text{imreconstruct}(I - h, I,\ \text{conn}) @f$.
///
/// @param I     Input image.
/// @param h     Suppression threshold.
/// @param conn  Connectivity.
/// @param mr    Memory resource (nullptr → process default).
/// @return      H-maxima image.
/// @see imhmin, imextendedmax
Value imhmax(const Value &I, double h, int conn,
             std::pmr::memory_resource *mr = nullptr);

/// @brief H-minima transform (`J = imhmin(I, h, conn)`) — dual of @ref imhmax.
///
/// @param I     Input image.
/// @param h     Suppression threshold.
/// @param conn  Connectivity.
/// @param mr    Memory resource (nullptr → process default).
/// @return      H-minima image.
/// @see imhmax
Value imhmin(const Value &I, double h, int conn,
             std::pmr::memory_resource *mr = nullptr);

/// @brief Extended-maxima transform (`J = imextendedmax(I, h, conn)`).
///
/// LOGICAL mask of regional maxima of @ref imhmax: keeps only peaks
/// with height `>= h` above their surroundings.
///
/// @param I     Input image.
/// @param h     Height threshold.
/// @param conn  Connectivity.
/// @param mr    Memory resource (nullptr → process default).
/// @return      LOGICAL extended-maxima mask.
/// @see imhmax, imextendedmin
Value imextendedmax(const Value &I, double h, int conn,
                    std::pmr::memory_resource *mr = nullptr);

/// @brief Extended-minima transform (`J = imextendedmin(I, h, conn)`).
///
/// Dual of @ref imextendedmax: troughs with depth `>= h`.
///
/// @param I     Input image.
/// @param h     Depth threshold.
/// @param conn  Connectivity.
/// @param mr    Memory resource (nullptr → process default).
/// @return      LOGICAL extended-minima mask.
/// @see imextendedmax
Value imextendedmin(const Value &I, double h, int conn,
                    std::pmr::memory_resource *mr = nullptr);

/// @brief Minima imposition (`J = imimposemin(I, BW, conn)`).
///
/// Modifies grayscale `I` so its only regional minima sit at the
/// marker pixels in `BW`. Soille's reconstruction-by-erosion recipe.
/// Useful as the pre-processing step for marker-controlled watershed.
///
/// @param I     Grayscale image.
/// @param BW    LOGICAL marker mask.
/// @param conn  Connectivity.
/// @param mr    Memory resource (nullptr → process default).
/// @return      Image with imposed minima.
/// @see imregionalmin, imreconstruct
Value imimposemin(const Value &I, const Value &BW, int conn,
                  std::pmr::memory_resource *mr = nullptr);

/// @brief Remove border-touching components
/// (`J = imclearborder(BW, conn)`).
///
/// `marker = BW restricted to the rim`, `R = imreconstruct(marker, BW, conn)`,
/// `J = BW & ~R`.
///
/// @param BW    Binary input.
/// @param conn  Connectivity.
/// @param mr    Memory resource (nullptr → process default).
/// @return      Binary image with rim components removed.
/// @see imkeepborder
Value imclearborder(const Value &BW, int conn,
                    std::pmr::memory_resource *mr = nullptr);

/// @brief Keep only border-touching components
/// (`J = imkeepborder(BW, conn)`) — dual of @ref imclearborder.
///
/// @param BW    Binary input.
/// @param conn  Connectivity.
/// @param mr    Memory resource (nullptr → process default).
/// @return      Binary image with only rim-connected components.
Value imkeepborder(const Value &BW, int conn,
                   std::pmr::memory_resource *mr = nullptr);

/// @brief White top-hat (`J = imtophat(I, SE) = I - imopen(I, SE)`).
///
/// Highlights bright structures smaller than `SE` on a dark background.
/// Output saturates to the input class.
///
/// @param I   Input image.
/// @param SE  Structuring element.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Top-hat image.
/// @see imbothat, imopen
Value imtophat(const Value &I, const Value &SE,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Black top-hat (`J = imbothat(I, SE) = imclose(I, SE) - I`).
///
/// Highlights dark structures smaller than `SE` on a bright background.
///
/// @param I   Input image.
/// @param SE  Structuring element.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Bottom-hat image.
/// @see imtophat, imclose
Value imbothat(const Value &I, const Value &SE,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Morphological gradient (`G = mmgradm(I, se_dil, se_ero)`).
///
/// Default: @f$ G = \text{imdilate}(I) - \text{imerode}(I) @f$ with
/// elementary cross (diamond, radius 1). For LOGICAL inputs the
/// subtraction collapses to `dilate & ~erode`.
///
/// **Half-gradients:**
/// - `mmgradm(I, Value::Empty, se_ero)` → internal (`-imerode`).
/// - `mmgradm(I, se_dil, Value::Empty)` → external (`imdilate`).
///
/// @param I       Input image.
/// @param se_dil  Dilation SE, or `Value::Empty` for internal half.
/// @param se_ero  Erosion SE, or `Value::Empty` for external half.
/// @param mr      Memory resource (nullptr → process default).
/// @return        Gradient image.
Value mmgradm(const Value &I, const Value &se_dil, const Value &se_ero,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Pack a binary image into UINT32 columns (`BWP = bwpack(BW)`).
///
/// Each 32 rows of `BW` become one UINT32; bit 0 corresponds to the
/// first row, bit 31 to the 32nd. Output rows = `ceil(M / 32)`.
/// Zero-padded if `M` is not a multiple of 32.
///
/// @param BW  LOGICAL image.
/// @param mr  Memory resource (nullptr → process default).
/// @return    UINT32 packed image.
/// @see bwunpack
Value bwpack(const Value &BW, std::pmr::memory_resource *mr = nullptr);

/// @brief Unpack a UINT32 binary image (`BW = bwunpack(BWP, M)`).
///
/// Inverse of @ref bwpack. `M` is the original row count; pass 0 for
/// `rows(BWP) * 32`.
///
/// @param BWP  UINT32 packed image.
/// @param M    Original row count.
/// @param mr   Memory resource (nullptr → process default).
/// @return     LOGICAL image.
/// @see bwpack
Value bwunpack(const Value &BWP, size_t M,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Neighbourhood-LUT filter (`A = applylut(BW, LUT)`).
///
/// LUT length must be `2^(n²)` for some `n`; common choices are
/// `n = 2` (length 16) and `n = 3` (length 512). Each output pixel
/// is the LUT entry indexed by the bit pattern of its `n × n`
/// neighbourhood (zero-padded), with kernel weights
/// `reshape(2^[nq-1:-1:0], n, n)`.
///
/// @param BW   Binary input.
/// @param LUT  Lookup table (length `2^(n²)`).
/// @param mr   Memory resource (nullptr → process default).
/// @return     Filtered image, same shape as `BW`.
/// @see bwmorph
Value applylut(const Value &BW, const Value &LUT,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Binary hit-or-miss transform (`J = bwhitmiss(BW, se1, se2)`).
///
/// @f$ J = \text{imerode}(BW, se_1)\ \wedge\ \text{imerode}(\sim BW, se_2) @f$.
/// `se1` marks foreground requirements; `se2` marks background
/// requirements. They must have disjoint neighbourhoods to be
/// meaningful (not enforced).
///
/// @param BW   Binary input.
/// @param se1  Foreground SE.
/// @param se2  Background SE.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Binary hit-or-miss output.
Value bwhitmiss(const Value &BW, const Value &se1, const Value &se2,
                std::pmr::memory_resource *mr = nullptr);

/// @brief Composite binary morphological operations
/// (`J = bwmorph(BW, op, n)`).
///
/// Faithful port of MATLAB R2025b's `bwmorph`. Each named operation
/// corresponds to a 512-entry LUT (or a chain of LUTs and bitwise
/// compositions), indexed by the 3×3 neighbourhood of every pixel.
/// Out-of-bounds pixels are 0.
///
/// Supported operations (case-sensitive — caller normalises):
/// - **single-LUT**: `"bridge"`, `"clean"`, `"diag"`, `"dilate"`,
///   `"endpoints"`, `"erode"`, `"fatten"`, `"fill"`, `"hbreak"`,
///   `"majority"`, `"perim4"`, `"perim8"`, `"remove"`.
/// - **composite chains**: `"bothat"`, `"close"`, `"open"`, `"tophat"`,
///   `"shrink"`, `"skeleton"`, `"spur"`, `"thin"`, `"thicken"`,
///   `"branchpoints"`.
///
/// @param BW  Binary input.
/// @param op  Operation name.
/// @param n   Iteration count. `n = -1` means "until stable"
///            (MATLAB's `Inf`); `n = 1` is the default.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Processed binary image.
/// @throws Error  Unknown `op` (`m:bwmorph:badOp`).
Value bwmorph(const Value &BW, const std::string &op, int n,
              std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::image
