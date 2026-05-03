# numkit-m bugs and known issues

Collected during the autonomous parity cycle (started 2026-05-03).
Append-only. Ядро (`core/`) is off-limits to this cycle, so several of
these are observations awaiting a separate `core/`-side pass.

Severity legend: **P0** crash / data loss; **P1** wrong result;
**P2** missing feature relative to MATLAB; **P3** test-only / style.

---

## 1. `core/`: VM resolution-order — user m-files don't shadow builtins  — **P1**

**Test:** `TW_VM/MFileResolverTest.MultiOutputMFileResolves/VM`
**File:** [core/tests/mfile_resolver_test.cpp:89](core/tests/mfile_resolver_test.cpp:89)
**Symptom:** Test creates a user `split.m` with `function [a, b] = split(x)`,
adds dir to path, calls `[a, b] = split(5)`. After the parity cycle
registered `split` as a builtin (commit `ef1d700`), VM started resolving
to the builtin (1 output) and failing the 2-output destructure. **TW
resolves correctly** to the user m-file in the same scenario.
**Root cause (probable):** VM's symbol-lookup order is `builtin → user`
instead of MATLAB's `user-on-path → builtin`.
**Workaround (this cycle):** documented; do not unregister `split`
because we want MATLAB parity. Rename the test fixture or fix VM
resolution — both are `core/` changes, deferred.
**First seen:** 2026-05-03, commit `ef1d700`.

---

## 2. `core/`: `eval('expr')` captured by outer assignment leaks `ans` display — **P2**

**Test:** `TW_VM/EvalRegressionTest.AssignmentCaptureSuppressesInnerAns/{TW,VM}`
**File:** [libs/builtin/tests/frame_introspection_test.cpp:474](libs/builtin/tests/frame_introspection_test.cpp:474)
**Symptom:** `r = eval('a + b');` should suppress the inner-expression
`ans` display when the outer assignment captures it (MATLAB
behaviour). Both TW and VM print `ans` — fails reliably on both
backends at HEAD `4eb6c22`, before any of my changes.
**Status:** pre-existing, not caused by parity cycle.
**First seen:** present at 2026-05-03 baseline.

---

## 3. `core/`: `eval([fname, '(x)'])` access-violation in loop inside function — **P0** flaky

**Test:** `TW_VM/EvalRegressionTest.BracketConcatInLoopInsideFunction/VM`
**File:** [libs/builtin/tests/frame_introspection_test.cpp:507-536](libs/builtin/tests/frame_introspection_test.cpp:507)
**Symptom:** A loop inside a user function does `v = eval([fname,
'(x)']);` per iteration. On VM (MSVC native) this triggers SEH
0xC0000005 access violation; on WASM it manifests as "Too many
input arguments". TW path is unaffected. Test's own header comment
calls it a known repro of an `eval` arg-cleanup bug between
successive calls in the same VM frame.
**Stability:** flaky — passes ~2/3 runs at baseline `4eb6c22`. Not
introduced by parity work.
**First seen:** in tree before 2026-05-03.

---

## 4. `libs/builtin`: `string()` doesn't accept cell-of-chars — **P2**

**Reproducer:** `string({'a','b','c'})` → "Cannot convert input to string".
**MATLAB:** returns a 1×3 string array.
**Impact:** Surfaced when writing parity bench specs for `join` —
forced us to use string-literal syntax `["a","b","c"]` instead of
the cell→string idiom that's common in MATLAB code.
**Where:** `libs/builtin/src/language/strings/strings.cpp` —
`toString()` rejects cells.
**First seen:** 2026-05-03 while specing `join` bench.

---

## 5. `libs/builtin`: indexed assignment to string-array elements unsupported — **P2**

**Reproducer:**
```matlab
arr = strings(1, 5);
arr(1) = "x";   % → Indexed assignment not supported for type 'string'
```
**MATLAB:** assigns the element.
**Impact:** Can't easily build string arrays element-by-element from
loops; users must concat via `["a","b",...]` literal.
**Where:** somewhere in indexing dispatch (likely `core/`).
**First seen:** 2026-05-03 while specing `join` bench.

---

## 6. `libs/builtin`: `repmat` doesn't accept `string` type — **P2**

**Reproducer:** `repmat("hi", 1, 100)` → "ND repmat does not support
type 'string'".
**MATLAB:** returns 1×100 string array.
**Where:** `libs/builtin/src/language/arrays/...` `repmat` code path.
**First seen:** 2026-05-03 while specing `join` bench.

---

## 7. `libs/builtin`: parens-indexing on `string` arrays unsupported — **P2**

**Reproducer:** `S = strings(2,3); S(1,1)` → "elemAt not supported for
type 'string'".
**MATLAB:** returns a string scalar.
**Workaround:** use `strlength(S(:))`-style aggregate operations.
**Where:** core indexing dispatch.
**First seen:** 2026-05-03 while smoke-testing the new `strings()`.

---

## 8. `libs/builtin`: 2-D string-array literal `["a","b"; "c","d"]` rejected — **P2**

**Reproducer:** `["a","b"; "c","d"]` → "Concatenation not supported for
type 'string' (in matrix construction)".
**MATLAB:** returns a 2×2 string array.
**Workaround:** none for 2-D string arrays; must use 1-D and reshape (also
unsupported for strings).
**Where:** matrix-literal dispatch in core, string-side concat helpers.
**First seen:** 2026-05-03 while specing `join` bench in 2-D form.

---

## 9. `libs/builtin`: most scalar trig / hyperbolic functions miss SIMD — **P3**

**Functions** (perf-only — correctness OK on all):
- inverse: `acos`, `asin`, `atan`, `atan2`
- hyperbolic: `sinh`, `cosh`, `tanh`
- direct (slow): `tan`
- degree variants: `sind`, `cosd`, `tand`
- multiple-of-π: `sinpi`, `cospi`
- coord helpers: `cart2pol`, `hypot` (and by extension `cart2sph` / `sph2cart`)

**Already SIMD-OK (parity-class):** `sin`, `cos` go through
`transcendental_simd.cpp` via Highway lanes.

**Symptom (parity-bench iterations 1 & 2):**

| function | numkit_ms | vs MATLAB |
|---|---:|---:|
| `sin`      |  0.85 | **1.07×** parity |
| `cos`      |  0.86 | **1.03×** parity |
| `tan`      |  7.28 | 0.12× |
| `atan2`    | 10.64 | 0.07× |
| `cart2pol` | 17.13 | 0.19× |
| `acos`     |  6.91 | 0.23× |
| `asin`     |  6.75 | 0.24× |
| `atan`     |  6.36 | 0.09× |
| `cosd`     | 10.73 | 0.09× |
| `cosh`     |  8.20 | 0.11× |
| `cospi`    |  9.24 | 0.07× |
| `hypot`    |  6.64 | 0.17× |
| `sind`     | 10.63 | 0.07× |
| `sinh`     |  8.34 | 0.15× |
| `sinpi`    |  9.11 | 0.09× |
| `tand`     | 10.18 | 0.09× |
| `tanh`     |  9.68 | 0.13× |

Correctness ULP-level OK on all (1M-point sweeps, element-wise SAVE
comparison vs MATLAB R2025b). The 5–15× perf gap is uniform: every
non-{sin,cos} trig falls through to scalar `std::*` because SIMD
lanes are only wired for sin / cos in
`libs/builtin/src/math/_backends/transcendental_simd.cpp`.

**Where:** `libs/builtin/src/math/trig/*.cpp` for the trig adapters;
`transcendental_simd.cpp` for the Highway plumbing. The pattern for
`sin` / `cos` is the template — adding `tan`, `asin`, `acos`, `atan`,
`atan2`, the hyperbolics, the `*d` and `*pi` variants, and `hypot` is
mechanical.

**Status:** **pending — libs fix.** Not blocking parity. Single
mass-SIMD pass should close all of these.

**First seen:** 2026-05-03, parity bulk-bench iterations 1 & 2.

---

## 10. `libs/signal`: `nextpow2` is scalar-only and namespaced — **P2**

**Reproducer:** `nextpow2([1, 2, 5, 100])` → "VM: undefined function 'nextpow2'".
After `import signal.*`: `nextpow2([1, 2, 5, 100])` → "Cannot convert
double to scalar (in call to 'nextpow2')".
**MATLAB:** unqualified, accepts any-shape input, returns same-shape output.
**Impact:** Surfaced when bulk-benching the `Logs` section — element-wise
spec on a 1M-pt array can't even run. Function is technically present
(libs/signal has it) but parity-incompatible.
**Where:** [libs/signal/src/library.cpp](libs/signal/src/library.cpp) registers it under `signal.*`;
implementation in [libs/signal/src/transforms/transform_helpers.cpp](libs/signal/src/transforms/transform_helpers.cpp)
takes a single double scalar.
**Fix:** lift to top-level (`core` namespace) and vectorize over input
shape — straightforward libs work, deferred from this cycle.
**First seen:** 2026-05-03, parity bulk-bench iteration 4.

---

## Notes

- This file is the bug intake for the parity cycle. When I close one
  (e.g. by fixing in `libs/`), the row stays for history with a
  "Fixed in commit X" line; new bugs go to the bottom.
- Bugs whose root cause sits in `core/` are tracked here for
  visibility but not actioned by this cycle.
- P3 perf gaps surfaced by bulk-bench are documented here too (see #9).
  They are not regressions; they're SIMD-optimization candidates.
