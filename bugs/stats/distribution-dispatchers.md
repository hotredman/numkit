# stats.cdf / pdf / icdf / random — generic distribution dispatchers missing

- **Status:** 🔴 OPEN
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

## Suggested fix
Thin dispatchers: map the distribution name (+ case-insensitive aliases:
'Normal'/'norm', 'Poisson'/'poiss', etc.) to the existing `<fam>cdf` /
`<fam>pdf` / `<fam>inv` / `<fam>rnd` and forward the trailing parameter
args. Mostly a name table + arg forwarding; the per-family math already
exists. Cover the families numkit already ships. Verify the alias table +
parameter order against MATLAB.

## References
- new file under `src/toolboxes/stats/src/...`
- shipped families: `normcdf/normpdf/norminv/normrnd`, `poisspdf`, etc.
- MATLAB `doc cdf`, `doc pdf`, `doc icdf`, `doc random`
