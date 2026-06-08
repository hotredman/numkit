# builtin.quadgk / integral2 / integral3 / quad2d — functions missing

- **Status:** 🔴 OPEN
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

## Root cause
Not implemented. numkit has `integral` (1-D adaptive) and `trapz`/`quad`
but not the Gauss-Kronrod or tensor-product N-D integrators.

## Suggested fix
- `quadgk(f,a,b)`: adaptive Gauss-Kronrod (15-point) with interval
  subdivision; supports infinite limits via variable substitution. Medium.
  Could share the error-estimate / subdivision loop with `integral`.
- `integral2(f,xa,xb,ya,yb)`: nested 1-D adaptive quadrature (the default
  `'method','auto'` → tiled). `integral3` extends one more dimension.
- `quad2d(f,...)`: tiled/vectorised 2-D — can wrap the same kernel.
All take a `FnHandle` callback (vectorised `f`). Verify the integral value
vs MATLAB on a polynomial + a Gaussian (closed-form references).

## References
- new file(s) under `toolboxes/builtin/src/math/...` (cf. existing `integral`)
- MATLAB `doc quadgk`, `doc integral2`, `doc integral3`, `doc quad2d`
