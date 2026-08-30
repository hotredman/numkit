# math.polyder — two-arg single-output form returns wrong polynomial

- **Status:** ✅ FIXED (2026-06-05)
- **Severity:** P2 (wrong result for a documented signature)
- **Kind:** bug
- **Found:** 2026-06-05 via DEEP-PROBE (poly/interp sweep, cycle 55)

## Fixed
- Fixed: 2026-06-05 (bug-fix loop, cycle 55),
  `src/math/src/poly/polynomials.cpp` (`polyder_reg`). MATLAB
  `polyder(a,b)` has two distinct meanings by `nargout`:
  - **1 output** → derivative of the PRODUCT `a*b`, i.e. `polyder(conv(a,b))`
    = `conv(a',b) + conv(a,b')`;
  - **2 outputs** `[q,d]` → quotient rule `d/dx(a/b)`,
    `q = conv(a',b) − conv(a,b')`, `d = conv(b,b)`.
  numkit's adapter returned the quotient NUMERATOR (`num`, the `−` form) for the
  single-output case too — so `polyder(a,b)` gave `conv(a',b) − conv(a,b')`
  instead of the product derivative `conv(a',b) + conv(a,b')`.
- Fix: in `polyder_reg`, the 2-arg `nargout <= 1` path now computes
  `polyder(conv(a,b))` (product derivative) via the existing `polyConv` +
  `polyderRaw` + `trimLeadingZeros` helpers. The 2-output quotient path and the
  1-arg form are unchanged.
- Verified vs MATLAB R2025b:
  `polyder([1 0],[1 1])` = `[2 1]` (d/dx[x(x+1)] = 2x+1);
  `polyder([1 2],[1 3])` = `[2 5]`; `polyder([1 0 0],[1 1])` = `[3 2 0]`;
  `[q,d]=polyder([1 0],[1 1])` → q=`1`, d=`[1 2 1]` (quotient, unchanged);
  `polyder([1 2 3])` = `[2 2]` (single-arg, unchanged).
- Live guard: `PolyTest.PolyderProductForm` (new) +
  `BuiltinKnownBug.PolyderProduct`. Parity:
  `tools/parity/specs/polyder_product.json` (correctness=OK). Smoke:
  `src/math/tests/smoke/polyder_product_smoke.m`.

## Symptom
`polyder(a,b)` (single output) returns the wrong polynomial — it computes the
quotient-rule numerator instead of the derivative of the product `a*b`.

## Repro
```matlab
polyder([1 0],[1 1])   % numkit: 1      ;  MATLAB: [2 1]   (d/dx[x*(x+1)])
polyder([1 2],[1 3])   % numkit: 1      ;  MATLAB: [2 5]
polyder([1 0 0],[1 1]) % numkit: [1 2 0];  MATLAB: [3 2 0]
[q,d] = polyder([1 0],[1 1])  % q=1, d=[1 2 1] on BOTH (quotient form OK)
```

## Root cause
`polyder_reg` always used the quotient numerator (`num`) for `outs[0]` when
two arguments were given, regardless of `nargout`; the single-output case
should instead return the product-rule derivative.

## References
- `src/math/src/poly/polynomials.cpp` (`polyder_reg`)
- MATLAB `doc polyder` (k = polyder(a,b) differentiates a*b; [q,d] = polyder(a,b)
  differentiates a/b)
