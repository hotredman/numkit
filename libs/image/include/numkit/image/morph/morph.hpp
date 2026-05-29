// libs/image/include/numkit/image/morph/morph.hpp
//
// Morphological operations.

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <string>
#include <vector>

namespace numkit { class Engine; }

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

/// @brief Nonlinear neighbourhood filter via lookup table
/// (`A = bwlookup(BW, lut)`).
///
/// Performs a 2×2 (`lut` length 16) or 3×3 (`lut` length 512) nonlinear
/// neighbourhood filtering operation on binary image `BW`. Each output
/// pixel is `lut` indexed by the bit pattern of its neighbourhood
/// (zero-padded at the border), with the same index convention as
/// @ref applylut / @ref makelut. The modern replacement for `applylut`,
/// restricted to the documented 16- and 512-element table sizes.
///
/// @param BW   Binary input (logical or numeric, treated as `BW != 0`).
/// @param lut  Lookup table — exactly 16 or 512 elements.
/// @param mr   Memory resource (nullptr → process default).
/// @return     Filtered image, same shape as `BW`, class of `lut`.
/// @throws Error  `lut` not 16 or 512 elements.
/// @see makelut, applylut
Value bwlookup(const Value &BW, const Value &lut,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Build a lookup table for @ref bwlookup
/// (`lut = makelut(fun, n)`).
///
/// Evaluates `fun` on every one of the `2^(n²)` binary `n × n`
/// neighbourhoods and returns the `2^(n²)`-element column vector of
/// results. `n` is 2 (length 16) or 3 (length 512). The neighbourhood
/// for table index `k` (0-based) has, in column-major order, position
/// `i` set to bit `(k >> (n²−1−i)) & 1` — the inverse of the
/// `reshape(2^[nq−1:−1:0], n, n)` weight kernel used by `bwlookup`, so
/// `bwlookup(BW, makelut(fun, n))` applies `fun` to each neighbourhood.
/// `fun` receives a logical `n × n` matrix and returns a scalar; the
/// output table is always DOUBLE.
///
/// @param eng  Engine used to invoke the function handle.
/// @param fun  Function handle: `(logical n×n) → scalar`.
/// @param n    Neighbourhood size (2 or 3).
/// @param mr   Memory resource (nullptr → process default).
/// @return     `2^(n²) × 1` DOUBLE lookup table.
/// @throws Error  `n` not 2 or 3, or `fun` returns a non-scalar.
/// @see bwlookup, applylut
Value makelut(numkit::Engine &eng, const Value &fun, int n,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Morphological operations on a binary volume
/// (`J = bwmorph3(V, operation)`).
///
/// Evaluates each voxel from its `3×3×3` neighbourhood (zero-padded at
/// the border). With `count` = number of set voxels in the
/// 27-neighbourhood *including the centre* and `faces6` = the six
/// 6-connected face neighbours:
///
/// | operation       | rule                                  |
/// |-----------------|---------------------------------------|
/// | `branchpoints`  | centre set **and** `count > 3`        |
/// | `clean`         | centre set **and** `count != 1`       |
/// | `endpoints`     | centre set **and** `count == 2`       |
/// | `fill`          | centre set **or** `faces6 == 6`       |
/// | `majority`      | `count > 13` (≥ 14 of 27)             |
/// | `remove`        | centre set **and** `faces6 != 6`      |
///
/// Accepts a 2-D image (treated as a single-plane volume) or a 3-D
/// volume; the output is always LOGICAL of the same size. Input is taken
/// as `V != 0`. Clean-room port of MATLAB R2025b's `bwmorph3` rules.
///
/// @param V    Binary volume (numeric or logical, 2-D or 3-D).
/// @param op   One of `branchpoints` / `clean` / `endpoints` / `fill` /
///             `majority` / `remove`.
/// @param mr   Memory resource (nullptr → process default).
/// @return     LOGICAL volume, same size as `V`.
/// @throws Error  Unknown operation, or rank > 3.
/// @see bwmorph, bwskel, imerode, imdilate
Value bwmorph3(const Value &V, const std::string &op,
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
/// Each named operation
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
///            (i.e. `Inf` iterations); `n = 1` is the default.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Processed binary image.
/// @throws Error  Unknown `op` (`m:bwmorph:badOp`).
Value bwmorph(const Value &BW, const std::string &op, int n,
              std::pmr::memory_resource *mr = nullptr);

/// @brief Trace object boundary in a binary image
/// (`B = bwtraceboundary(BW, P, fstep, conn, m, dir)`).
///
/// Moore-Neighbor boundary tracing starting from foreground pixel
/// `P` and initial search direction `fstep`. The trace walks the
/// 8-connected (default) or 4-connected boundary and emits row/col
/// coordinates.
///
/// **fstep semantics.** `fstep` defines the direction of the
/// notional "previous" pixel as the OPPOSITE of `fstep`, so the
/// search starts one position clockwise of that previous direction.
/// This matches MATLAB R2025b behaviour even when `fstep` points
/// into the object interior (the search just sweeps around to the
/// first valid boundary neighbour).
///
/// **Termination.** Stops when the boundary loop closes (we
/// revisit `P` after ≥ 1 step), when `m` pixels have been emitted,
/// or when no neighbours exist (isolated pixel → returns
/// `[P; P]`).
///
/// Reference: Moore-Neighbor tracing, Pavlidis 1982,
/// *Algorithms for Graphics and Image Processing*, §7.5.
///
/// @param BW       2-D binary mask (logical or numeric non-zero).
/// @param P        `[row, col]` 1-based starting boundary pixel.
/// @param fstep    `"N"` / `"NE"` / `"E"` / `"SE"` / `"S"` /
///                 `"SW"` / `"W"` / `"NW"`. For `conn = 4`, only
///                 `"N"` / `"E"` / `"S"` / `"W"`.
/// @param conn     `8` (default) or `4`.
/// @param m        Maximum number of pixels to extract
///                 (`std::numeric_limits<size_t>::max()` for Inf).
/// @param dir      `"clockwise"` (default) or `"counterclockwise"`.
/// @param mr       Memory resource (nullptr → process default).
/// @return         `Q × 2` DOUBLE matrix of `[row, col]` pixels.
Value bwtraceboundary(const Value &BW, const Value &P,
                      const std::string &fstep, int conn,
                      std::size_t m, const std::string &dir,
                      std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::image
