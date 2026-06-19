# math.quadgk / integral2 / integral3 / quad2d — functions missing

- **Status:** ✅ FIXED (2026-06-19) — nested adaptive 1-D integral
- **Severity:** P2 (missing functions)
- **Kind:** missing-fn
- **Found:** 2026-06-04 via missing-fn sweep

## Symptom
The N-D / adaptive numerical-integration family beyond the basic 1-D
`integral` is not registered. `quadgk` (adaptive Gauss-Kronrod, 1-D),
`integral2`/`integral3` (2-D / 3-D), and `quad2d` (tiled 2-D) are all
undefined. These are MATLAB *core* (not a toolbox) — widely used.

## Repro
```matlab
quadgk(@(x)exp(-x.^2),0,1)            % MATLAB: 0.746824132812427
integral2(@(x,y)x.*y,0,1,0,1)         % MATLAB: 0.25
integral3(@(x,y,z)x+y+z,0,1,0,1,0,1) % MATLAB: 1.5
quad2d(@(x,y)x.*y,0,1,0,1)            % MATLAB: 0.25
% numkit (each): Error — VM: undefined function '<name>'
```

## Fix (2026-06-19)
Implemented all four on top of numkit's existing 1-D adaptive Gauss-Kronrod
`integral`:
- `integral2(fn,a,b,c,d)` = **iterated quadrature** — an outer `integral`
  over `x` whose integrand, at each node, runs an inner `integral` over `y`
  of `fn(x,·)`. Done purely by composing `FnHandle`s in the math layer
  (`integration.cpp`): the inner 1-arg callback wraps the user's 2-arg
  `fn` with the captured `x`. `integral3` triple-nests.
- `quadgk(fn,a,b)` = the 1-D `integral` (adaptive Gauss-Kronrod); returns
  `[q, errbnd]` (errbnd = the tolerance, conservative).
- `quad2d(fn,...)` = the older name for `integral2`.

`'AbsTol'` is accepted. For smooth integrands the iterated approach matches
MATLAB's tiled `integral2` to tolerance. Works on **both backends** (the
nested callback re-entry runs under TreeWalker and the VM).

Verified vs MATLAB R2025b (parity `integral2.json` → OK):
`integral2(x·y,[0,1]²)=0.25`, `integral2(exp(x·y),[0,1]²)=1.317902151454`,
`integral3(x+y+z,[0,1]³)=1.5`, `quadgk(exp(−x²),0,1)=0.746824132812427`,
`quad2d(x·y)=0.25`. Guards: `integration_test.cpp`
(`NumericalIntegrationND`, dual-engine) + `known_bugs_test.cpp`
(`NumericalIntegrationND`, promoted live); smoke `integral2_smoke.m`.

## References
- `src/math/src/integration/integration.cpp` (`integral2`, `integral3`),
  `.../include/numkit/math/integration/integration.hpp`,
  `src/bundle/src/register/math/integration_reg.cpp` (`integral2_reg`,
  `integral3_reg`, `quadgk_reg`, `quad2d_reg`),
  `src/bundle/src/builtin_library.cpp` (registration).
- `tools/parity/specs/integral2.json`.
- reused: the 1-D adaptive `integral` (Gauss-Kronrod)
- MATLAB `doc quadgk`, `doc integral2`, `doc integral3`, `doc quad2d`
