# stats.cdf / pdf / icdf / random — generic distribution dispatchers missing

- **Status:** ✅ FIXED (2026-06-18) — cdf/pdf/icdf/random name-dispatchers added
- **Severity:** P2 (missing functions)
- **Kind:** missing-fn
- **Found:** 2026-06 via DEEP-PROBE

## Symptom
The generic distribution functions that take a distribution NAME and
dispatch to the specific family are all undefined: `cdf`, `pdf`, `icdf`,
`random` (and likely `makedist`/`truncate`/`mle`-family companions).

## Repro
```matlab
cdf('Normal', 1, 0, 1)        % numkit: undefined function 'cdf'   (MATLAB 0.84134)
pdf('Poisson', 2, 3)          % numkit: undefined function 'pdf'   (MATLAB 0.22404)
icdf('Normal', 0.975, 0, 1)   % numkit: undefined function 'icdf'  (MATLAB 1.95996)
random('Normal', 0, 1, 1, 3)  % numkit: undefined function 'random'
```

## Root cause
Not implemented. numkit HAS the per-family functions (`normcdf`, `poisspdf`,
`norminv`, `normrnd`, …) — only the generic name-dispatching wrappers are
missing.

## Fix (2026-06-18)
Implemented as register-level glue (no new compute): a name → family-prefix
table + a `findExternal` forward to the existing per-family builtin. `cdf` →
`<fam>cdf`, `pdf` → `<fam>pdf`, `icdf` → `<fam>inv`, `random` → `<fam>rnd`;
args after the name are forwarded unchanged. Covers all 23 shipped families
(every one has cdf/pdf/inv/rnd), case-insensitive with the common aliases
('Normal'/'norm'/'gaussian', 'Chisquare'/'chi-square'/'chi2', …). Unknown names
throw. The per-family math is pre-existing + parity-validated, so the dispatcher
inherits its accuracy.

Verified vs MATLAB R2025b (parity `dist_dispatchers.json` → OK):
`cdf('Normal',1,0,1)=0.841345`, `cdf('Gamma',2,3,1)=0.323324`,
`pdf('Poisson',2,3)=0.224042`, `pdf('Binomial',2,5,0.3)=0.3087`,
`icdf('Normal',0.975,0,1)=1.959964`, `icdf('Chisquare',0.95,3)=7.814728`;
`random('Normal',0,1,2,3)` → 2×3 (RNG, size-checked). Guards:
`dist_dispatch_test.cpp`; smoke `dist_dispatch_smoke.m`.

`makedist`/`truncate`/generic `mle` companions remain separate gaps (not part
of this change).

## References
- `src/bundle/src/register/stats/distributions/dist_dispatch_reg.cpp`
  (`cdf_reg`/`pdf_reg`/`icdf_reg`/`random_reg`, registered under `dist`/`compat`).
- `tools/parity/specs/dist_dispatchers.json`.
- shipped families: `normcdf/normpdf/norminv/normrnd`, `poisspdf`, etc.
- MATLAB `doc cdf`, `doc pdf`, `doc icdf`, `doc random`
