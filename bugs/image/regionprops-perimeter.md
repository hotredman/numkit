# image.regionprops — unknown property silently dropped (Perimeter etc.)

- **Status:** 🔴 OPEN
- **Severity:** P1 (silently wrong — no error, no field)
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
computing them or raising "unsupported property". `libs/image/src/.../regionprops*`.

## Suggested fix
Two parts: (a) **immediate, cheap** — make an unknown/unimplemented property
name throw a clear "regionprops: property 'X' is not supported in this
revision" instead of silently dropping it (prevents the misleading
downstream error). (b) **larger** — implement Perimeter (boundary tracing +
corner-weighted distance: straight steps 1, diagonal steps √2, MATLAB's
specific weighting) and the other deferred fields. Do (a) now, (b) as
separate items.

## References
- `libs/image/src/...regionprops...`
- MATLAB `doc regionprops`
