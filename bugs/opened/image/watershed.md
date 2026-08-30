# image.watershed — function missing

- **Status:** 🔴 OPEN (DEFERRED 2026-06-19 — exact MATLAB labels/ridges not
  reconstructible from black-box; see attempt note below)
- **Severity:** P2 (missing function)
- **Kind:** missing-fn
- **Found:** 2026-06 via DEEP-PROBE

## Symptom
`watershed` (watershed transform for segmentation) is not registered.

## Repro
```matlab
clear;
L = watershed(magic(5))
% numkit: Error — VM: undefined function 'watershed'
% MATLAB: label matrix with 0 ridge lines separating catchment basins
```

## Root cause
Not implemented.

## ⚠️ Attempt + DEFER (2026-06-19) — textbook immersion diverges from MATLAB
Prototyped the **Vincent-Soille immersion** (1991, the canonical
sort-by-level + FIFO-geodesic-distance flooding) in MATLAB with
8-connectivity and column-major tie-breaking, and compared to MATLAB's
`watershed` on 5 cases. Result: it matches MATLAB **only** on the trivial
ones (single-basin, a 1-D valley, a Gaussian bump) and **diverges** on the
two that matter:

- `magic(5)`: basin COUNT matches (3) but the **label numbering** and
  **ridge pixels** differ (MATLAB labels the left column basin `1`; the
  immersion's minimum-detection order labels it `3`, and the 0-ridge runs on
  different pixels).
- distance-transform of two blobs (`-bwdist(~B)`): MATLAB produces a **thick
  anti-diagonal SKIZ ridge** (the full equidistant band of 0s); the textbook
  immersion produces a **thin (≈1-pixel) watershed**. The ridge *thickness*
  is structurally different.

So MATLAB's `watershed` is **not** the plain Vincent-Soille immersion — its
label-ordering and (especially) its ridge/SKIZ convention come from a
different/proprietary variant that can't be pinned from black-box outputs.
Matching the exact label matrix would need MATLAB's source. **Deferred** —
the basin segmentation is reconstructible but the exact labels/ridges are not.
Don't repeat the plain-immersion attempt.

## Suggested fix (if revisited)
Reverse-engineer MATLAB's exact ridge convention (thick SKIZ) +
label-ordering, or accept a documented non-parity (`correctness=N/A`,
validate basin COUNT + topology only). Pairs with the distance-transform
workflow (`bwdist` + `imhmin`, both shipped).

## References
- new file under `src/toolboxes/image/src/...`
- shipped: `bwdist`, `imhmin`, `bwlabel`, `imreconstruct`
- MATLAB `doc watershed`
