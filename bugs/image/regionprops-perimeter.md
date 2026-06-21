# image.regionprops — unknown property silently dropped (Perimeter etc.)

- **Status:** ✅ FIXED (2026-06-18) — Perimeter implemented; unknown properties now error
- **Severity:** P1 (silently wrong — no error, no field)
- **Kind:** bug
- **Found:** 2026-06 via DEEP-PROBE

## Symptom
Requesting a property numkit doesn't implement (e.g. `'Perimeter'`)
**does not error** — `regionprops` returns a struct that simply lacks the
field. The user only discovers this when a later `.Perimeter` access fails
with a confusing "non-existent field" error. MATLAB computes the property.

## Repro
```matlab
s = regionprops(logical(ones(3,3)), 'Perimeter');
s.Perimeter
% numkit: Error — Reference to non-existent field 'Perimeter'
%         (the regionprops call itself did NOT throw)
% MATLAB: s.Perimeter = 7.476000   (boundary distance, corner-weighted)
```
Deferred/large geometric fields in the same boat:
`Perimeter, Solidity, ConvexArea, EulerNumber, FilledArea, Circularity,
Extrema, ConvexHull, Orientation?` (verify which are still missing).

## Root cause
The property-name parser ignores unrecognized names instead of either
computing them or raising "unsupported property". `src/toolboxes/image/src/.../regionprops*`.

## Fix (2026-06-18)
Both parts done:
- **(a)** `regionprops` now validates every requested property name against the
  implemented set and throws `regionprops: property 'X' is not supported in this
  revision` for unknown/unimplemented names (Solidity, ConvexArea, EulerNumber,
  FilledArea, Circularity, Extrema, ConvexHull, …) instead of silently dropping
  them. (`'basic'` is also now honored as the {Area, Centroid, BoundingBox}
  shortcut.)
- **(b)** Perimeter implemented (`regionPerimeter` in region.cpp): trace the
  region's outer 8-connected boundary (Moore-neighbor, from the top-left-most
  pixel) and apply MATLAB R2025b's `computePerimeterFromBoundary` estimator
  `0.980·Ne + 1.406·No − 0.091·Nc` (Ne axis steps, No diagonal, Nc orientation
  changes around the closed loop; Vossepoel-Smeulders). Value is invariant to
  trace start/direction. Single-pixel → 0; outer boundary only (holes not
  traced — MATLAB likewise warns Perimeter is for hole-free regions).

Verified numkit == MATLAB R2025b exactly: 3×3 solid 7.476, 4×4 solid 11.396,
plus 5.624, eye(4) diagonal 8.436. (Parity harness reports N/A for regionprops —
pre-existing, same class as histcounts; confirmed by a direct MATLAB run.)
Guards: `src/toolboxes/image/tests/image_batch3_test.cpp`
(`RegionpropsShapeDescriptors` — values + unsupported-property throw) +
`src/toolboxes/image/tests/known_bugs_test.cpp` (`RegionpropsPerimeter`,
promoted live).

## References
- `src/toolboxes/image/src/region/region.cpp` (`regionPerimeter`, property
  validation in `regionprops`).
- Spec `tools/parity/specs/regionprops_perimeter.json`; smoke
  `src/toolboxes/image/tests/smoke/regionprops_perimeter_smoke.m`.
- MATLAB `doc regionprops`; the estimator is `computePerimeterFromBoundary` in
  `toolbox/images/images/regionprops.m`.
