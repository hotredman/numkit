# Clean-room specification — `adapthisteq`

Contrast-Limited Adaptive Histogram Equalisation (CLAHE). Written per
`cleanroom/PROTOCOL.md` (Spec Author role).

> **Scope.** Full MATLAB-API parity: the clean-room rewrite supports the
> complete `adapthisteq` argument set — `NumTiles`, `ClipLimit`,
> `NBins`, `Range`, `Distribution` (uniform / rayleigh / exponential),
> `Alpha`. The current numkit function only does `uniform` + `Range
> 'full'`; this spec also *completes* those gaps. CLAHE has
> implementation-specific details (clip-redistribution, interpolation
> rounding); the goal is to be functionally equivalent to MATLAB, not
> necessarily bit-identical — if parity falls outside tolerance, the
> tests are re-baselined.

## Public references

- K. Zuiderveld, "Contrast Limited Adaptive Histogram Equalization",
  in *Graphics Gems IV* (P. S. Heckbert, ed.), Academic Press, 1994,
  pp. 474–485. ACM DOI 10.5555/180895.180940 — the tile-based CLAHE
  with bilinear interpolation between tile mappings.
- S. M. Pizer, R. E. Johnston, J. P. Ericksen, B. C. Yankaskas,
  K. E. Muller, "Contrast-Limited Adaptive Histogram Equalization:
  Speed and Effectiveness", Proc. 1st Conf. on Visualization in
  Biomedical Computing, IEEE, 1990 (UNC TR 90-035) — the
  contrast-limiting step (clip + redistribute).
  PDF: https://www.cs.unc.edu/techreports/90-035.pdf
- S. M. Pizer et al., "Adaptive Histogram Equalization and Its
  Variations", *Computer Vision, Graphics, and Image Processing*
  39:355–368, 1987 — the AHE family and non-uniform target
  distributions.

## 1. Algorithm (CLAHE — tile variant)

Input: a 2-D greyscale image `I` (RGB / N-D → error, as MATLAB).

1. **Tiling.** Partition `I` into `numTilesR × numTilesC` rectangular
   tiles ("contextual regions"). Pad `I` symmetrically (mirror
   reflection) beforehand so each dimension divides evenly into the
   tile count; strip the padding from the result at the end.

2. **Per-tile histogram.** For each tile, histogram its pixels into
   `NBins` bins spanning the working intensity range.

3. **Contrast limiting** (Pizer 1990): cap each histogram bin at a
   *clip limit* and redistribute the clipped excess.
   - `numPix` = pixels per tile.
   - `minLimit = numPix / NBins` (the flat-histogram height).
   - `clipCount = minLimit + ClipLimit · (numPix − minLimit)`
     — so `ClipLimit = 0` ⇒ ordinary uniformisation, `ClipLimit = 1`
     ⇒ maximum contrast.
   - Clip every bin to `clipCount`; sum the removed excess `E`.
   - Redistribute `E` uniformly: add `floor(E / NBins)` to every bin,
     then hand out the remaining `E mod NBins` one count at a time
     across the bins. (If a bin is pushed back over `clipCount` this
     is acceptable — a single redistribution pass, per Pizer 1990.)

4. **Per-tile mapping function.** Form the cumulative sum of the
   clipped histogram and the cumulative probability
   `P[b] = cumsum[b] / numPix ∈ [0, 1]`. Convert `P` to an output
   intensity via the target **Distribution** (§3), giving a lookup
   table `map_t : bin → output intensity` per tile `t`.

5. **Bilinear interpolation** (Zuiderveld 1994). Each output pixel is
   produced by bilinearly interpolating the mapping functions of the
   four nearest tile centres:
   - interior pixels: weighted blend of 4 tile maps;
   - pixels in the border half-tiles: blend of 2 tile maps;
   - the four corner regions: the single corner tile's map directly.
   Implement via the standard `(numTilesR+1) × (numTilesC+1)` grid of
   interpolation cells, with the row/column weights given by the
   pixel's fractional position between the two tile centres.

## 2. `Range` argument

Defines the output intensity span `[outMin, outMax]`:
- `"full"` (default) — the full range of the image's class
  (`[0, 1]` for floating point; the class range for integer types).
- `"original"` — `[min(I), max(I)]` of the actual input pixels.

The per-tile mapping (§1.4 / §3) is scaled into `[outMin, outMax]`.

## 3. `Distribution` argument — target histogram shape

The mapping converts the cumulative probability `P` to an output
intensity. `Alpha` is the shape parameter (default 0.4) for the
non-flat distributions.

- `"uniform"` (default): `y = outMin + P · (outMax − outMin)`.
- `"rayleigh"`: Rayleigh inverse CDF —
  `y = outMin + sqrt( 2·Alpha² · ln( 1 / (1 − P) ) )`, then clamp to
  `[outMin, outMax]`.
- `"exponential"`: exponential inverse CDF —
  `y = outMin − (1/Alpha) · ln( 1 − P )`, then clamp to
  `[outMin, outMax]`.

(For `P = 1` the rayleigh/exponential logs diverge — clamp `P` just
below 1, or clamp the resulting `y`, so the top bin maps to `outMax`.)

## 4. Interface (numkit)

```cpp
Value adapthisteq(const Value &I, const AdaptHistEqOptions &opts,
                  std::pmr::memory_resource *mr);
```

`AdaptHistEqOptions` (in `numkit/image/contrast/contrast.hpp`) — the
existing struct, **plus one new field**:

```cpp
struct AdaptHistEqOptions {
    int         numTilesR   = 8;
    int         numTilesC   = 8;
    double      clipLimit   = 0.01;
    int         nBins       = 256;
    std::string distribution = "uniform";   // uniform | rayleigh | exponential
    double      alpha       = 0.4;
    std::string range       = "full";       // NEW — full | original
};
```

Validation / errors (keep the existing `m:adapthisteq:*` identifiers):
- `numTilesR`, `numTilesC` each ≥ 2 → else `m:adapthisteq:badTiles`.
- `clipLimit ∈ [0, 1]` → else `m:adapthisteq:badClip`.
- `nBins ≥ 2` → else `m:adapthisteq:badBins`.
- `distribution ∈ {uniform, rayleigh, exponential}` → else error.
- `range ∈ {full, original}` → else error.
- 2-D greyscale input only; RGB / N-D → `m:adapthisteq:unsupportedShape`.

The registration `adapthisteq_reg` already parses every NV key
(`NumTiles`, `ClipLimit`, `NBins`, `Distribution`, `Alpha`, `Range`);
it must be updated to **store** the `Range` value into `opts.range`
(currently it throws for `Range != "full"`) and the distribution/range
deferral throws are removed.

PMR HARD RULE: scratch via `ScratchArena` / `ScratchVec<T>`; the
returned Value uses `mr`.

## 5. Verification

- gtest: `libs/image/tests/adapthisteq_*` — covers the C++ API and the
  engine path. Re-baseline hardcoded expectations to the clean-room
  output where they differ.
- Parity: `tools/parity/specs/` entry covering `adapthisteq` — require
  `correctness = OK`; if the clean-room CLAHE falls outside tolerance
  vs MATLAB (likely, given MATLAB's undocumented clip/interp details),
  re-baseline as was done for SRH/PEF and record it in the spec
  comment.
- **Add a MATLAB-independent correctness test** (mandatory — see
  PROTOCOL): e.g. a low-contrast synthetic image (values in a narrow
  sub-range) must come out with a visibly wider spread — assert the
  output's dynamic range / std. dev. is substantially larger than the
  input's, and that `ClipLimit = 0` enhances less than `ClipLimit`
  large.

## Constraints for the Implementer

- Do **not** open `libs/image/src/contrast/contrast.cpp`.
- Do **not** consult MATLAB `.m` source or any third-party reference
  implementation (in particular not the Graphics Gems `clahe.c`).
- Implement solely from this specification and the cited public
  references.
