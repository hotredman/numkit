# math.histcounts — automatic binning unsupported (edges required)

- **Status:** ✅ FIXED (2026-06-18) — full MATLAB binpicker auto-binning ported
- **Severity:** P2 (missing input form)
- **Kind:** stub
- **Found:** 2026-06 via DEEP-PROBE

## Symptom
`histcounts(x)`, `histcounts(x, nbins)`, and the `'BinWidth'` / `'BinLimits'`
forms throw — numkit requires explicit bin edges. MATLAB auto-selects bins.

## Repro
```matlab
[N, e] = histcounts([1 2 2 3 3 3])
% numkit: Error — histcounts: bin edges required — automatic binning
%         (nbins / 'BinWidth' / 'BinLimits') is not supported
% MATLAB: N = [1 2 3],  edges = [0.5 1.5 2.5 3.5]
[N, e] = histcounts([1 2 3 4 5 6 7 8 9 10], 3)
% MATLAB: N = [3 4 3]
```

## Root cause
The auto-binning rule is not implemented; only the explicit-edges path
exists.

## Fix (2026-06-18)
Ported MATLAB R2025b's auto-binning verbatim into `histcountsAutoEdges`
(`discrete.cpp`): the `binpicker` / `binpickerbl` "nice-edge" choosers, the
`integerrule`, and the `autorule` (integer bins for integer-valued data of
range ≤ 50, else Scott's rule) + `scott`/`fd`/`sturges`/`sqrt` rules — sourced
by dumping `toolbox/matlab/datafun/histcounts.m`, `…/+matlab/+internal/+math/
binpicker.m`, `…/private/integerrule.m`. `histcounts_reg` now accepts 1 arg
(`histcounts(x)`), reads a scalar 2nd arg as the bin COUNT (vs a vector =
explicit edges), and parses `'NumBins'` / `'BinWidth'` / `'BinLimits'` plus all
`'BinMethod'` values. `histcounts([1 2 2 3 3 3])` → `[1 2 3]`; `histcounts(
1:10, 3)` → `[3 4 3]`.

Note: `histcounts(x, 1)` is now a single bin (MATLAB's bin-COUNT semantics),
where it previously threw treating `[1]` as a bad edge vector.

Validated against MATLAB R2025b by a direct run of the parity expr — 16/16
fingerprints match (auto-integer / NumBins / Scott / BinWidth / BinLimits /
sturges / sqrt / fd). The parity *harness* reports N/A for histcounts (a
pre-existing limitation shared by the committed histcounts_binedges/_integers
specs), so the cross-check was done directly. Guards:
`src/bundle/tests/known_bugs_test.cpp` (`HistcountsAutoBins` / `HistcountsNbins`,
promoted live) + `src/lang/tests/setops_test.cpp` (`HistcountsAutomaticBinning`).

## References
- `src/math/src/discrete/discrete.cpp` (`histcountsAutoEdges` + binpicker port),
  `src/math/include/numkit/math/discrete/discrete.hpp` (`HistBinSpec`),
  `src/bundle/src/register/math/discrete_reg.cpp` (`histcounts_reg`).
- Spec `tools/parity/specs/histcounts_autobin.json`;
  smoke `src/lang/tests/smoke/histcounts_autobin_smoke.m`.
- MATLAB `doc histcounts` (automatic binning algorithm).
