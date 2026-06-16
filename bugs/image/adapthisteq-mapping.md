# adapthisteq (CLAHE) output is systematically ~54% too bright vs MATLAB

- **Status:** 🔴 OPEN
- **Severity:** P2 (implemented + accepts all options, but values diverge badly from MATLAB)
- **Kind:** bug
- **Found:** 2026-06-14 building a parity spec for adapthisteq (weld-defect pipeline review).

## Symptom

`adapthisteq` runs and accepts every documented option (`ClipLimit`,
`NumTiles`, `Distribution` uniform/rayleigh, uint8 + double[0,1]), but the
contrast-limited equalisation MAPPING diverges from MATLAB R2025b by a large
margin — the output is shifted bright and the low end is compressed (does not
reach toward 0). This is a CDF-mapping / clip-redistribution / tile-CDF
normalisation error, not rounding.

## Repro

```matlab
[xx, yy] = meshgrid(1:64, 1:64);
I = uint8(120 + 40*sin(xx/8) + 30*cos(yy/6));   % deterministic textured image
J = adapthisteq(I);                              % defaults (uniform, ClipLimit 0.01, NumTiles [8 8])
```

| fingerprint            | numkit | MATLAB R2025b |
|------------------------|--------|---------------|
| `J(32,32)`             | 205    | **128**       |
| `J(1,1)`               | 174    | **100**       |
| `min(J(:))`            | 127    | **20**        |
| `max(J(:))`            | 255    | **235**       |
| `sum(J(:))`            | 823957 | **533825** (+54%) |
| rayleigh `Jr(32,32)`   | 193    | **118**       |
| double `Jd(32,32)`     | 0.8032 | **0.5027**    |

MATLAB roughly preserves/centres the mean and spreads to ~[20,235]; numkit
pushes the mean way up (~201 vs ~130) and collapses the bottom half to ≥127.
The pure-gradient degenerate case is even worse (all-255).

## Suspected cause — likely a REGRESSION

`tools/parity/PROGRESS.md` previously recorded adapthisteq as a **"faithful port
of MATLAB R2025b adapthisteq.m … tol=0 bit-exact on every probed pixel"** with
**"Distribution != 'uniform' deferred (throws)"**. Today rayleigh runs (no
throw) AND the uniform path is far off — so adding the non-uniform
distributions almost certainly broke the per-tile CDF mapping that was once
bit-exact. First suspect: the change that introduced rayleigh/exponential.

The per-tile cumulative-histogram → intensity mapping: output never descending
below ~mid-range suggests the tile CDF is no longer anchored at the display
minimum (lowest populated bin should map toward 0), or the clip-excess
redistribution / inter-tile bilinear blend got mis-scaled. Reference
(Zuiderveld 1994): per-tile clipped histogram → CDF normalised so cdf(min)→0,
cdf(max)→full-range → bilinear interpolation between the 4 neighbouring tiles.

## Guards

- Parity spec `tools/parity/specs/adapthisteq.json` — committed, currently
  reports MISMATCH vs MATLAB R2025b (tracks the regression in PROGRESS.md).
- `ImageKnownBug.DISABLED_AdapthisteqMapping` in
  `toolboxes/image/tests/known_bugs_test.cpp` — asserts the MATLAB values
  (`J(32,32)≈128`, `min(J)≈20`, `max(J)≈235`).

When fixed: drop `DISABLED_`, flip this md to ✅, and the parity spec should
report `correctness=OK`.
