# image.corner — corner-point detection missing (cornermetric exists)

- **Status:** ✅ FIXED (2026-06-19) — peak detection on the shipped cornermetric
- **Severity:** P2 (missing function)
- **Kind:** missing-fn
- **Found:** 2026-06-04 via missing-fn sweep

## Symptom
`corner(I)` — return the `[x y]` coordinates of detected corner points — is
not registered. The underlying response map `cornermetric` **is** present in
numkit, so the missing piece is the peak-finding / thresholding layer on top
of it.

## Repro
```matlab
I = zeros(20,20); I(6:15,6:15) = 1;   % a bright square block
C = corner(I);
% MATLAB: C is 4x2 (the 4 block corners), first row [6 6]
% numkit: Error — VM: undefined function 'corner'
```

## Root cause
Not implemented. `corner` = compute `cornermetric` (Harris or
minimum-eigenvalue, already shipped) → find local maxima → threshold →
return the strongest `N` (default 200) corner coordinates as `[x y]`.

## Fix (2026-06-19)
Wrapped the shipped `cornermetric` (`corner` in `object/object.cpp`,
`corner_reg` in `object/object_reg.cpp`):
1. `cmetric = cornermetric(I, method, k, fc)` (defaults: Harris, k=0.04,
   the 5-tap Gaussian).
2. Local-maximum mask: pixels `> QualityLevel·max(cmetric)` (default 0.01)
   that are `≥` every in-bounds 8-neighbor.
3. Connected components (8-conn) discovered in column-major order →
   per-component centroid + peak strength.
4. Stable sort by strength **descending**; ties keep column-major (MATLAB
   `find`) order. Take up to `N` (default 200).
5. Return `K×2` integer `[x y] = [col row]` (centroid rounded).

The image border is excluded **naturally** — the cornermetric there is
`≤ 0 < threshold` (confirmed by probing: a block at the image edge yields
only its inner corner).

**Ordering subtlety:** MATLAB sorts by strength (a strong corner at a high
column beats a weak one at a low column — `corner(W,1)` returns the strong
one), and its cornermetric is **bit-exactly symmetric**, so a symmetric
square's four equal-strength corners keep `find` order. numkit's
cornermetric matches MATLAB only to ~1e-8 and carries ~1-ULP asymmetry at
symmetric corners, which a raw strength sort would surface as a wrong order.
Fix: quantise the strength to `~1e-9·max` before the stable sort — ULP
noise collapses to one key (→ column-major order, as MATLAB) while genuinely
distinct strengths still separate.

Verified vs MATLAB R2025b: bright square → 4 corners `[6 6;6 15;15 6;15 15]`;
two squares (contrast 1 vs 0.5) → 8 with the strong square first; `corner(A,2)`
truncates; `corner(W,1)` → `[20 20]` (strength beats position); edge block →
1 corner; `MinimumEigenvalue` → 4. Parity `corner.json` → OK.

## References
- `src/toolboxes/image/src/object/object.cpp` (`corner`),
  `include/numkit/image/object/object.hpp`,
  `src/bundle/src/register/image/object/object_reg.cpp` (`corner_reg`),
  `image_library.cpp` (reg)
- `tools/parity/specs/corner.json`,
  `src/toolboxes/image/tests/corner_test.cpp` (8 cases),
  `known_bugs_test.cpp` (`Corner`, promoted live),
  smoke `tests/smoke/corner_smoke.m`
- reused: the shipped `cornermetric`
- MATLAB `doc corner`
