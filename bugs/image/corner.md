# image.corner — corner-point detection missing (cornermetric exists)

- **Status:** 🔴 OPEN
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

## Suggested fix
Wrap the existing `cornermetric`: non-maximum suppression on the metric map,
keep peaks above `QualityLevel·max`, sort by strength, return up to `N`
`[col row]` coordinates. Small-medium given `cornermetric` exists. Verify
the corner count + coordinates on a synthetic square vs MATLAB.

## References
- new file under `libs/image/src/...`; reuse the shipped `cornermetric`
- MATLAB `doc corner`
