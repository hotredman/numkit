# wavelet.upcoef — direct coefficient reconstruction missing

- **Status:** ✅ FIXED (2026-06-19) — synthesis cascade
- **Severity:** P2 (missing function)
- **Kind:** missing-fn
- **Found:** 2026-06-04 via missing-fn sweep (split from wavelet/wenergy on 2026-06-19)

## Symptom
`upcoef` (direct reconstruction of approximation/detail coefficients up N
levels) is not registered. (Split from the original wenergy/upcoef entry;
`wenergy` is now fixed — see wavelet/wenergy.md.)

## Repro
```matlab
y = upcoef('a', 5, 'db1', 2);
% MATLAB: y = [2.5 2.5 2.5 2.5]
% numkit: Error — VM: undefined function 'upcoef'
```

## Fix (2026-06-19)
Implemented `numkit::wavelet::upcoef` (`multilevel.cpp`). `upcoef(O, X,
wname, N[, L])` runs the synthesis cascade `N` times: each level
**interleaves zeros** (`[x0, 0, x1, 0, …, x_{n-1}]`, length `2n−1`) and
**full-convolves** with the reconstruction lowpass `Lo_R` — except the first
level of a detail branch (`O = "d"`), which uses the highpass `Hi_R`.
Filters come from `wavelet_filters(wname)`. **Not idwt:** there is no
half-length trim, so a level grows the length to `2n−1 + |F|−1` (Haar's
`|F|=2` gives the clean doubling, longer wavelets keep the filter tail). The
optional `L` keeps the central `L` samples; `N = 0` returns `X`.

Verified vs MATLAB R2025b (parity `upcoef.json` → OK): `upcoef('a',5,'db1',
2)=[2.5 2.5 2.5 2.5]`; `upcoef('d',5,'db1',2)=[2.5 2.5 −2.5 −2.5]` (detail
sign flip from `Hi_R`); `upcoef('a',[1 2],'db2',1)=[0.482963 0.836516
1.190074 1.543628 0.448288 −0.258819]` (6 taps); `upcoef('a',[1 2 3],'db1',
1)=[0.7071 0.7071 1.4142 1.4142 2.1213 2.1213]`. Guards: `upcoef_test.cpp`
(6 TEST_F: approx/detail/db2/vector/N=0/bad-type), `known_bugs_test.cpp`
(`Upcoef`, promoted live); smoke `upcoef_smoke.m`. **Closes the wenergy/
upcoef cluster.**

## References
- `src/toolboxes/wavelet/src/dwt/multilevel.cpp` (`upcoef`, `convFull`),
  `.../include/numkit/wavelet/dwt/multilevel.hpp`,
  `src/bundle/src/register/wavelet/dwt/multilevel_reg.cpp` (`upcoef_reg`).
- `tools/parity/specs/upcoef.json`.
- shipped + reused: `wavelet_filters` (wfilters)
- related: wavelet/wenergy.md (the energy half, also fixed)
- MATLAB `doc upcoef`
