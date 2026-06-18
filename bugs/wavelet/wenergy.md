# wavelet.wenergy — energy distribution of a 1-D decomposition

- **Status:** ✅ FIXED (2026-06-19) — band energy percentages
- **Severity:** P2 (missing function)
- **Kind:** missing-fn
- **Found:** 2026-06-04 via missing-fn sweep

> Split note: this entry originally also tracked `upcoef` (direct coefficient
> reconstruction), which needs the idwt-style upsampling cascade. That piece
> is its own entry now — see bugs/wavelet/upcoef.md (still OPEN).

## Symptom
`wenergy` (percentage of energy per approximation/detail level) is not
registered.

## Repro
```matlab
[C, L] = wavedec([1 2 3 4 5 6 7 8], 2, 'db1');
[Ea, Ed] = wenergy(C, L);
% MATLAB: Ea = 95.0980392157, Ed = [0.98039216 3.92156863]
% numkit: Error — VM: undefined function 'wenergy'
```

## Fix (2026-06-19)
Implemented `numkit::wavelet::wenergy` (`multilevel.cpp`, next to
`wavedec`/`detcoef`): with total energy `‖C‖²`, the approximation
percentage is `Ea = 100·‖cA_N‖²/‖C‖²` (the first `L(1)` coefficients), and
each detail percentage is `Ed(i) = 100·‖cD‖²/‖C‖²`.

**Ordering gotcha:** the `C` vector packs details coarsest-first
(`cA_N, cD_N, …, cD_1`), but MATLAB's `Ed` is indexed by **level number**
(`Ed(1)` = level 1 = finest, `Ed(N)` = level N = coarsest) — the reverse of
the `C` walk. (Confirmed against MATLAB: 1-D ramp 1:16 db1 L3 → Ed =
[0.267, 1.069, 4.278]; the finest detail `Ed(1)=0.267` carries the least
energy and the coarsest `Ed(3)=4.278` the most for a ramp.)
`Ea + sum(Ed) = 100`.

Verified vs MATLAB R2025b (parity `wenergy.json` → OK): `[1..8]` db1 L2 →
Ea=95.0980392157, Ed=[0.98039216, 3.92156863]; `1:16` db1 L3 → Ea=94.385,
Ed=[0.267, 1.069, 4.278]; `sin(1:32)` db2 L2 → Ea=34.1876, Ed=[9.833,
55.979]. Guards: `wenergy_test.cpp` (4 TEST_F), `known_bugs_test.cpp`
(`Wenergy`, promoted live); smoke `wenergy_smoke.m`.

## References
- `src/toolboxes/wavelet/src/dwt/multilevel.cpp` (`wenergy`),
  `.../include/numkit/wavelet/dwt/multilevel.hpp` (`WenergyResult`),
  `src/bundle/src/register/wavelet/dwt/multilevel_reg.cpp` (`wenergy_reg`).
- `tools/parity/specs/wenergy.json`.
- related: wavelet/upcoef.md (the reconstruction half, still open)
- MATLAB `doc wenergy`
