# Clean-room specification — `bwmorph`

The binary-morphology operation dispatcher (Image Processing toolbox).
Written per `cleanroom/PROTOCOL.md` (Spec Author role).

> **Scope.** Functional equivalence to MATLAB R2025b `bwmorph(BW, op,
> n)` over its full documented operation set. This rewrite covers the
> **dispatcher** — the per-operation pipeline logic and the iteration
> framework. The 512-entry neighbourhood lookup tables it consumes are
> *reference data* supplied separately (see §3); they are the unique
> truth tables of the documented operations and are not part of what
> the implementer writes.

## Public references

- R. C. Gonzalez & R. E. Woods, *Digital Image Processing*, 4th ed.,
  Pearson, 2018 — Ch. 9, binary morphology: dilation, erosion, opening,
  closing, hit-or-miss, boundary extraction, thinning, thickening,
  skeletons, pruning.
- W. K. Pratt, *Digital Image Processing*, 4th ed., Wiley, 2007 —
  §14, morphological image processing; conditional / lookup-table
  ("mark / 3×3 template") pixel operations.
- L. Lam, S.-W. Lee, C. Y. Suen, "Thinning Methodologies — A
  Comprehensive Survey", *IEEE Trans. PAMI* 14(9):869–885, 1992 —
  parallel thinning by sub-iteration.
- MATLAB R2025b `doc bwmorph` — the documented per-operation
  definitions (the observable behaviour this spec targets).

## 1. Interface

```cpp
Value bwmorph(const Value &BW, const std::string &op, int n,
              std::pmr::memory_resource *mr = nullptr);
```

- **`BW`** — a 2-D binary image. 3-D input → error
  `m:bwmorph:unsupportedShape`. Every element is read as logical
  (`!= 0`).
- **`op`** — the operation name (already lower-cased by the caller).
- **`n`** — iteration count: apply the operation `n` times; `n = -1`
  is the "until stable" sentinel (MATLAB's `Inf`); `n = 0` is a no-op
  (return the input as logical). The default at the call site is 1.
- Output — a `LOGICAL` image the same size as `BW`.
- Unknown `op` → error `m:bwmorph:badOp`.

## 2. The 3×3 neighbourhood index

Every operation is, at its core, a 3×3-neighbourhood lookup. For a
pixel at `(r, c)` form a 9-bit index into a 512-entry table. The bit
layout is MATLAB's `makelut` convention — bit `k` is the neighbour at
row offset `(k mod 3) − 1` and column offset `(k div 3) − 1`:

```
      bit 0   bit 3   bit 6
      bit 1   bit 4   bit 7      (bit 4 = the centre pixel)
      bit 2   bit 5   bit 8
```

A neighbour outside the image reads as `0`. The lookup primitive —
call it `bwlookup3x3(src, dst, R, C, lut)` — computes, for every
pixel, this 9-bit index and writes `dst = lut[index]`.

## 3. The lookup tables (reference data — NOT to be authored)

The 512-entry tables are supplied as `constexpr std::array<uint8_t,
512>` in the header `libs/image/src/morph/bwmorph_luts.h`. Each table
is the unique truth table of a documented operation — for every one of
the 512 possible 3×3 binary neighbourhoods it gives the resulting
centre value. There is exactly one correct table per operation (the
operation's definition and its tabulation merge), so the tables are
reference facts, not authored code; `#include` the header and use them
by name. The available tables:

- single-pass operation tables: `lutdilate`, `luterode`, `lutbridge`,
  `lutclean`, `lutdiag`, `lutendpoints`, `lutfatten`, `lutfill`,
  `luthbreak`, `lutmajority`, `lutper4`, `lutper8`, `lutremove`,
  `lutbranchpoints`;
- thinning sub-tables: `lutthin1`, `lutthin2`;
- skeleton sub-tables: `lutskel1` … `lutskel8`;
- support tables: `lutshrink`, `lutspur`, `lutsingle`, `lutiso`,
  `lutbackcount4`.

## 4. Operations

All buffers below are column-major `uint8` logical (`0`/`1`), size
`R×C`, `N = R·C`. "Apply T" means one `bwlookup3x3` pass with table
`T`. Each operation transforms the working image `bw` in place; the
iteration framework (§5) calls it.

### 4.1 Single-table operations

`dilate`, `erode`, `bridge`, `clean`, `diag`, `endpoints`, `fatten`,
`fill`, `hbreak`, `majority`, `perim4`, `perim8`, `remove` — each is
one pass of the like-named table (`dilate`→`lutdilate`, `perim4`→
`lutper4`, `perim8`→`lutper8`, etc.).

### 4.2 Composite operations

- **`open`** — erosion followed by dilation: apply `luterode`, then
  apply `lutdilate` to the result.
- **`close`** — dilation followed by erosion: apply `lutdilate`, then
  `luterode`.
- **`bothat`** — bottom-hat: `close(bw) AND NOT bw`. Compute
  `close` into a temporary, then `bw ← temp AND (NOT bw)`.
- **`tophat`** — top-hat: `bw AND NOT open(bw)`. Compute `open` into a
  temporary, then `bw ← bw AND (NOT temp)`.
- **`thin`** — one pass = apply `lutthin1`, then apply `lutthin2` to
  the result.
- **`skeleton`** (alias `skel`) — one pass = apply the eight skeleton
  sub-tables `lutskel1` … `lutskel8` **in sequence**, each on the
  output of the previous.

### 4.3 `shrink`

One pass is four sub-iterations on a checkerboard. For sub-iteration
`s = 0 … 3`:

1. Apply `lutshrink` to `bw` → `m`.
2. Form the candidate result `cand = bw AND (NOT m)`.
3. Overwrite `bw` with `cand` **only at pixels on one checkerboard
   sub-field** — the pixels whose `(row, col)` parities equal
   `(rOff, cOff)`, stepping by 2 from the offsets. The four
   sub-fields, in order `s = 0,1,2,3`, use offsets
   `(rOff,cOff) = (0,0), (1,1), (1,0), (0,1)`.

(Pixels outside the active sub-field keep their current value; this is
the standard parallel-thinning sub-iteration scheme.)

### 4.4 `spur`

One pass:

1. Complement: `bw ← NOT bw`.
2. Apply `lutspur` to `bw` → `endPoints` (the spur end-pixels).
3. Sub-field 0 — offsets `(0,0)`: `bw ← bw XOR endPoints` on that
   sub-field only.
4. For sub-fields 1, 2, 3 — offsets `(1,0)`, `(0,1)`, `(1,1)`: first
   re-apply `lutspur` to the current `bw` → `e`; form `e AND
   endPoints`; XOR that into `bw` on the sub-field only.
5. Complement back: `bw ← NOT bw`.

### 4.5 `thicken`

One pass:

1. **Isolated-pixel boost.** Apply `lutiso` to `bw` → `iso` (isolated
   foreground pixels). If `iso` has any set pixel: apply `lutdilate`
   to `iso` → `grow`; apply `lutsingle` to `bw` → `oneNbr` (pixels
   with exactly one neighbour); then set `bw[i] = 1` wherever
   `oneNbr[i] AND grow[i]`.
2. **Padded thinning of the complement.** Build a buffer `c` of size
   `(R+4)×(C+4)`, all `1`; write `NOT bw` into its centre `R×C`
   region (offset `(2,2)`). Then: apply `lutthin1` to `c` → `c1`;
   apply `lutthin2` to `c1` → `c2`; apply `lutdiag` to `c2` → `d`.
   Update `c ← (c AND (NOT c2) AND d) OR c2`. Force the outer
   two-pixel border of `c` back to `1` (all four edges). Finally
   write the result back: `bw ← NOT (centre R×C region of c)`.

### 4.6 `branchpoints`

One pass:

1. `Cset` = apply `lutbranchpoints` to `bw`.
2. `Bset` = apply `lutbackcount4` to `bw` (a per-pixel small integer).
3. Per pixel: `E = (Bset == 1)`; `FC = (NOT E) AND Cset`;
   `Vp = (Bset == 2) AND (NOT E)`; `Vq = (Bset > 2) AND (NOT E)`.
4. `Dset` = apply `lutdilate` to `Vq`.
5. `M = FC AND Vp AND Dset`.
6. Result: `bw ← FC AND (NOT M)`.

## 5. Iteration framework

`bwmorph(BW, op, n, mr)`:

1. Validate shape (2-D only); pack `BW` into a column-major logical
   buffer.
2. `n == 0` → return the packed input as a `LOGICAL` Value unchanged.
3. Otherwise repeat: save the current image as `prev`; run one pass of
   `op`; if the pass result equals `prev`, stop (the image is stable);
   if `n` is finite and the requested count is reached, stop. For the
   "until stable" case (`n = -1`) keep a large safety cap (e.g.
   10000 passes) so a non-converging operation cannot loop forever.
4. Return the final image as a `LOGICAL` Value the same size as `BW`.

Note that a single "pass" of `thin` / `skeleton` / `shrink` / `spur`
is the whole multi-table / multi-sub-iteration procedure of §4 — the
iteration framework repeats *that* until stable or `n` times.

## 6. numkit interface & rules

- Signature exactly as §1 (unchanged from the existing header).
- PMR HARD RULE: working buffers via `ScratchArena` / `ScratchVec<T>`;
  the returned `Value` is allocated on `mr`.
- `#include "bwmorph_luts.h"` for the reference tables.
- The 3×3 lookup primitive and the boolean element-wise helpers
  (`and`, `or`, `not`, `xor`, `and-not`, `equal`) are part of what you
  write.

## 7. Verification

- gtest `libs/image/tests/bwmorph_test.cpp` — keep the existing cases;
  re-baseline hardcoded values only where a clean-room result genuinely
  differs (it should not — the tables are fixed facts and the
  pipelines are deterministic).
- Parity `tools/parity/specs/image_bwmorph.json` — require
  `correctness = OK` vs MATLAB R2025b on all 23 probed operations
  (`tol = 0`).
- **MATLAB-independent correctness test** (mandatory): on a known
  shape, assert the *defining* property of representative operations —
  e.g. `dilate` never removes a foreground pixel and `erode` never
  adds one (`dilate(BW) ⊇ BW ⊇ erode(BW)`); `perim4`/`perim8` output
  is a subset of `BW`; `thin`/`skeleton` of a solid block is a subset
  of the block with strictly fewer pixels; `bwmorph(BW, op, 0)`
  returns `BW` unchanged.

## Constraints for the Implementer

- Do **not** open `libs/image/src/morph/morph.cpp` — it contains code
  you must not see.
- Do **not** consult MATLAB `.m` source (`bwmorph.m`, `algbwmorph.m`,
  the `lut*.m` files) or any third-party reference implementation.
- You MAY (and must) `#include` the reference table header
  `libs/image/src/morph/bwmorph_luts.h` — it is data, not code.
- Implement the dispatcher solely from this specification and the
  cited public references.
