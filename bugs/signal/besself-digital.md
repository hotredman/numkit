# signal.besself — default path returned binomial garbage (ran digital)

- **Status:** ✅ FIXED (lib-dev, 2026-06)
- **Severity:** P1 (wrong result)
- **Kind:** bug
- **Found:** 2026-06 via DEEP-PROBE

## Symptom
`besself(n, Wo)` without the `'s'` flag returned the binomial coefficients
of `(s+Wo)^n` (all poles at −Wo) instead of the Bessel filter. Only
`besself(n, Wo, 's')` gave the correct analog filter.

## Repro (pre-fix)
```matlab
besself(3, 1)        % numkit (pre-fix): a = [1 3 3 1]   (WRONG, (s+1)^3)
                     % MATLAB:           a = [1 2.432881 2.466212 1]
besself(2, 1)        % numkit (pre-fix): a = [1 2 1]     MATLAB: [1 1.732051 1]
```

## Root cause
`besself_reg` (`libs/signal/src/filter_design/iir_designs.cpp`) passed the
parsed `analog` flag (false unless `'s'`) into `besself()`, which then ran
the DIGITAL bilinear path. But MATLAB `besself` is **always analog** — there
is no digital Bessel filter (the bilinear transform destroys the
maximally-flat group delay that defines a Bessel filter). The digital path
collapsed the (correct) `besselap` poles into `(s+Wo)^n`.

## Fix
Force `analog = true` in `besself_reg` regardless of the `'s'` flag (the flag
is now redundant). The (already-correct) `besselap` prototype + `lp2lp`/
`lp2hp` path then produces the right coefficients. 4 artefacts: impl +
parity correctness=OK (besself.json strengthened from a numel-only check to
real coefficient fingerprints) + gtest (besself_test.cpp) + smoke.

## References
- `libs/signal/src/filter_design/iir_designs.cpp` (besself_reg)
- `tools/parity/specs/besself.json`, `libs/signal/tests/besself_test.cpp`
- MATLAB `doc besself` ("does not support digital filters")
