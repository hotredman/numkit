# builtin.max / min — elementwise on complex errors "Not a double array"

- **Status:** 🔴 OPEN
- **Severity:** P2 (missing feature; numkit errors, no silent-wrong result)
- **Kind:** stub
- **Found:** 2026-06-17 via complex-input fusion work (probe of per-op complex)

## Symptom
Elementwise `max(A,B)` / `min(A,B)` (and hence `clamp`-style `max(lo,min(hi,z))`)
throw `Not a double array` when an operand is complex. MATLAB supports them:
compare by magnitude `|z|`, ties broken by phase `angle(z)` (max → larger angle,
min → smaller), and a NaN-component operand is omitted (the other wins).

The single-arg REDUCTION `max(z)` / `min(z)` already works (by magnitude).

## Repro
```matlab
max([3+4i, 0.2-0.1i], 1)   % MATLAB: [3+4i, 1]      ; numkit: ERROR "Not a double array"
min([3+4i, 0.2-0.1i], 1)   % MATLAB: [1, 0.2-0.1i]  ; numkit: ERROR
max(1+0i, 0+1i)            % MATLAB: 0+1i (tie |·|=1 → larger angle) ; numkit: ERROR
max(1+1i, 1-1i)            % MATLAB: 1+1i            ; numkit: ERROR
max(complex(NaN,2), 3+4i)  % MATLAB: 3+4i (NaN omitted) ; numkit: ERROR
```

## Root cause (two layers)
1. **Per-op:** `numkit::math::max/min(const Value&, const Value&, mr)`
   (`math/src/arithmetic/reductions.cpp`) go straight to `dispatchIntegerBinaryOp`
   then `elementwiseDouble(...fmax/fmin)`, which calls `doubleData()` on the
   complex operand → throws. No complex branch.
2. **Dispatch (the blocker):** adding a complex branch to `reductions.cpp` max/min
   was NOT enough — a diagnostic `throw` placed at the very top of
   `math::max(Value,Value,mr)` did NOT fire for `max(z,1)`, while a marker in the
   builtin handler `max_reg` (`bundle/.../register/math/reductions_reg.cpp`) DID
   fire. So `max_reg`'s `max(a0,a1,mr)` call is reached but does not reach
   `reductions.cpp:181` — the symbol binds elsewhere through the VM call path
   (likely the inline-builtin id-22/23 fast path in `core/src/vm.cpp`
   `execCallBuiltin`, or a layering using-re-export shim). The math-layer fix is
   shadowed. Verified the per-op fix compiles + links fresh (single reductions.obj,
   no stale dup) yet still isn't invoked — confirmed it is a dispatch problem, not
   a build-staleness one.

The MATLAB comparator (validated R2025b) for the eventual fix:
```
cmp(a,b,isMax): if a has a NaN component → b (and vice-versa; both NaN → NaN);
  else by |a| vs |b| (isMax: larger, min: smaller); tie → by angle(a) vs angle(b).
```

## Suggested fix
Resolve which call path actually computes `max(z,1)` for complex (instrument the
VM inline-builtin id 22/23 handler in `execCallBuiltin` — its array/non-scalar
fallback must route complex to the registered builtin, not a real-only loop).
Then add the complex comparator to `math::max/min(Value,Value,mr)` (and the
omitnan variants `maxOmitNanBinary`/`minOmitNanBinary`). `clamp` follows for free
(it is `max`/`min` composition). NOTE: numkit's per-op `max/min` reduction path
already has a complex |z|+angle comparator (`reductions_detail.hpp` ~line 289) to
reuse.

## References
- `math/src/arithmetic/reductions.cpp` (max/min binary), `reductions_detail.hpp`
- `core/src/vm.cpp` `execCallBuiltin` (inline builtin id 22=max, 23=min)
- `bundle/src/register/math/reductions_reg.cpp` (max_reg/min_reg)
- Related (closed in the same effort): complex floor/ceil/round/fix + expm1 now
  work (commit b2f30c53); complex clamp fusion is declined until this lands.
