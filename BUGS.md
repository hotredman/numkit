# numkit-m bugs and known issues

Collected during the autonomous parity cycle (started 2026-05-03).
Append-only. Ядро (`core/`) is off-limits to this cycle, so several of
these are observations awaiting a separate `core/`-side pass.

Severity legend: **P0** crash / data loss; **P1** wrong result;
**P2** missing feature relative to MATLAB; **P3** test-only / style.

---

## 1. `core/`: VM resolution-order — user m-files don't shadow builtins  — ✅ FIXED 2026-05-11

**Test:** `TW_VM/MFileResolverTest.MultiOutputMFileResolves/VM`
**File:** [core/tests/mfile_resolver_test.cpp:89](core/tests/mfile_resolver_test.cpp:89)
**Symptom (was):** Test creates a user `split.m` with `function [a, b] = split(x)`,
adds dir to path, calls `[a, b] = split(5)`. After the parity cycle
registered `split` as a builtin (commit `ef1d700`), VM resolved to the
builtin (1 output) and failed the 2-output destructure. TW resolved
correctly to the user m-file in the same scenario.
**Root cause:** VM's CALL / CALL_MULTI dispatch tried `findExternal`
(builtin) BEFORE `lookupUserFunction` (user m-file). MATLAB does the
opposite.
**Fix:** swap the two blocks in core/src/vm.cpp CALL and CALL_MULTI cases.
The compiled-cache check stays first (fast path for already-compiled
user fns); after that, m-file lookup runs, and only if that misses do
we fall through to builtins. Full gtest: 8443 PASS / 11 pre-existing
FAIL (down from 12 — this test was the 12th).
**First seen:** 2026-05-03, commit `ef1d700`.

---

## 2. `core/`: `eval('expr')` captured by outer assignment leaks `ans` display — **P2** ✅ FIXED 2026-05-11

**Test:** `TW_VM/EvalRegressionTest.AssignmentCaptureSuppressesInnerAns/{TW,VM}`
**File:** [libs/builtin/tests/frame_introspection_test.cpp:474](libs/builtin/tests/frame_introspection_test.cpp:474)
**Symptom:** `r = eval('a + b');` should suppress the inner-expression
`ans` display when the outer assignment captures it (MATLAB
behaviour). Both TW and VM print `ans` — fails reliably on both
backends at HEAD `4eb6c22`, before any of my changes.
**Status:** pre-existing, not caused by parity cycle.
**First seen:** present at 2026-05-03 baseline.
**Fix (2026-05-11):** Added `suppressTopLevelDisplay` flag to both
`Engine::eval(code)` and `Engine::eval(code, scope)` overloads
([core/include/numkit/core/engine.hpp](core/include/numkit/core/engine.hpp),
[core/src/engine.cpp](core/src/engine.cpp)). When set, a helper
walks the freshly-parsed AST and flips `suppressOutput=true` on
each top-level statement, which both TW and VM already gate their
DISPLAY emission on — single hook covers EXPR_STMT, ASSIGN,
FIELD_ASSIGN, etc. The `eval` builtin
([libs/builtin/src/library.cpp:3076](libs/builtin/src/library.cpp:3076))
passes `suppress = (nargout >= 1)` so bare `eval('a+b')` still
displays ans, but `r = eval('a+b')` doesn't. Side-effect prints
inside called functions (disp, fprintf, …) are unaffected — those
originate inside CALL nodes whose own statement-level flag isn't
touched.

---

## 3. `core/`: `eval([fname, '(x)'])` access-violation in loop inside function — **P0** ✅ FIXED 2026-05-11

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
**Root cause (2026-05-11):** Not eval arg-cleanup as the header
guessed — `VM::dispatchLoop` captures a reference into the
per-chunk call cache:
```cpp
auto &resolvedFuncs = chunkCallCache_[frame.chunk];
```
The `eval` builtin re-enters `Engine::eval` → `VM::execute`. On
re-entry, `VM::startExecution` calls `chunkCallCache_.clear()`,
which destroys the cache node `resolvedFuncs` referenced. After
the inner returns, the outer dispatch loop resumes with a dangling
reference. Reading it as a vector header sometimes gives `size==0`
(falls through harmlessly, test passes) and sometimes gives garbage
that triggers SEH on the next CALL — explaining the ~9/10 pass
rate. `savePausedState` rescued frames / forStack / tryStack /
registers across re-entry but missed the call cache.
**Fix:** Added `chunkCallCache` to `PausedState`
([core/include/numkit/core/vm.hpp:241-256](core/include/numkit/core/vm.hpp:241))
and move-save it in `savePausedState`, move-restore it in
`restorePausedState`
([core/src/vm.cpp:167-211](core/src/vm.cpp:167)). `unordered_map`
move preserves node addresses, so the outer's `resolvedFuncs`
reference reclaims live memory after the inner exits. Stress-test:
100/100 pass post-fix (was 9/10 baseline).

---

## 4. `libs/builtin`: `string()` doesn't accept cell-of-chars — **P2** ✅ FIXED

**Reproducer:** `string({'a','b','c'})` → "Cannot convert input to string".
**MATLAB:** returns a 1×3 string array.
**Impact:** Surfaced when writing parity bench specs for `join` —
forced us to use string-literal syntax `["a","b","c"]` instead of
the cell→string idiom that's common in MATLAB code.
**Where:** `libs/builtin/src/language/strings/strings.cpp` —
`toString()` rejects cells.
**First seen:** 2026-05-03 while specing `join` bench.
**Fix (2026-05-03):** Added a CELL branch to `toString` that walks
the source cell elementwise (each must be char / string / numeric
scalar) and packages into an N-element string array of the same
shape.

---

## 5. `libs/builtin`: indexed assignment to string-array elements unsupported — **P2** ✅ FIXED

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
**Fix (2026-05-03):** Added STRING branches to `writeElem` /
`writeScalar` in [core/src/value.cpp]. Source value can be string,
char, or numeric scalar — all go through `stringElemSet`.

---

## 6. `libs/builtin`: `repmat` doesn't accept `string` type — **P2** ✅ FIXED

**Reproducer:** `repmat("hi", 1, 100)` → "ND repmat does not support
type 'string'".
**MATLAB:** returns 1×100 string array.
**Where:** `libs/builtin/src/language/arrays/...` `repmat` code path.
**First seen:** 2026-05-03 while specing `join` bench.
**Fix (2026-05-03):** `repmatND` in
[libs/builtin/src/language/arrays/manip.cpp] now has a STRING/CELL
path (the byte-memcpy fast path can't apply to vector<Value>-backed
storage). Walks output indices, mods back to source via per-axis
modulo strides, and copies via `stringElemSet` / `cellAt`. The 2-D
`repmat` entry point delegates to ND for non-DOUBLE types.

---

## 7. `libs/builtin`: parens-indexing on `string` arrays unsupported — **P2** ✅ FIXED

**Reproducer:** `S = strings(2,3); S(1,1)` → "elemAt not supported for
type 'string'".
**MATLAB:** returns a string scalar.
**Workaround:** use `strlength(S(:))`-style aggregate operations.
**Where:** core indexing dispatch.
**First seen:** 2026-05-03 while smoke-testing the new `strings()`.
**Fix (2026-05-03):** Added STRING cases to `Value::elemAt`,
`Value::indexGet`, and `Value::indexGet2D` in [core/src/value.cpp].
Each builds a fresh string scalar / array and copies via
`stringElemSet`. 3-D / ND slicing still routes through the existing
infrastructure (CELL-style; STRING ND would need similar additions).

---

## 8. `libs/builtin`: 2-D string-array literal `["a","b"; "c","d"]` rejected — **P2** ✅ FIXED

**Reproducer:** `["a","b"; "c","d"]` → "Concatenation not supported for
type 'string' (in matrix construction)".
**MATLAB:** returns a 2×2 string array.
**Workaround:** none for 2-D string arrays; must use 1-D and reshape (also
unsupported for strings).
**Where:** matrix-literal dispatch in core, string-side concat helpers.
**First seen:** 2026-05-03 while specing `join` bench in 2-D form.
**Fix (2026-05-03):** Added a STRING branch to `Value::vertcat` in
[core/src/value.cpp] (horzcat already had one). `["a","b"; "c","d"]`
now correctly assembles a 2×2 string array. Char / scalar operands
auto-promote to single-element strings to match horzcat's existing
behavior.

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

## 10. `libs/signal`: `compat.nextpow2` is scalar-only — **P2** ✅ FIXED

**Reproducer:**
```matlab
import compat.*
nextpow2(100)            % → 7  (scalar OK)
nextpow2([1 2 5 100])    % → "Cannot convert double to scalar (in call to 'nextpow2')"
```
**MATLAB:** `nextpow2` is unqualified and vectorized — accepts any-shape
input and returns same-shape output.
**Impact:** Element-wise bulk-bench on a 1M-pt array can't run; the
compat alias only covers the scalar case.
**Where:** [libs/signal/src/library.cpp](libs/signal/src/library.cpp)
aliases `signal.nextpow2` into `compat`; implementation in
[libs/signal/src/transforms/transform_helpers.cpp](libs/signal/src/transforms/transform_helpers.cpp)
takes a single `double` scalar.
**Fix:** vectorize the impl (broadcast `nextpow2` element-wise over any
shape). Straightforward libs work, deferred from this cycle.
**Note (2026-05-03):** The earlier flag here that "nextpow2 isn't found
without `import signal.*`" was wrong — `import compat.*` flattens it,
and the parity harness now injects that line on every numkit run.
**First seen:** 2026-05-03, parity bulk-bench iteration 4.
**Fix (2026-05-03):** Added a vectorized `nextpow2(mr, const Value &)`
overload in [libs/signal/src/transforms/transform_helpers.cpp] that
applies the elementary `ceil(log2(|x_i|))` rule elementwise (returning
0 for |x_i| <= 0). The `nextpow2_reg` adapter now passes the whole
Value through; scalar callers still hit the same fast path via the
isScalar branch.

---

## 11. `core/` or `libs/`: `arrayfun(@lambda, vec)` ignores the lambda body — **P1** ✅ FIXED

**Reproducer:**
```matlab
arrayfun(@(x) x*2, 1:5)     % numkit: [1 2 3 4 5]   ; MATLAB: [2 4 6 8 10]
arrayfun(@(x) x^2, 1:5)     % numkit: [1 2 3 4 5]   ; MATLAB: [1 4 9 16 25]
arrayfun(@(x) sin(x), 1:5)  % numkit: [1 2 3 4 5]   ; MATLAB: sin([1..5])

% IMPORTANT — sibling functions are NOT broken (verified 2026-05-03):
cellfun(@(x) x*2, {1,2,3,4,5})            % numkit: [2 4 6 8 10] OK
structfun(@(x) x*2, struct('a',1,'b',2))  % numkit: [2;4]        OK
```
**Symptom:** The anonymous-function body is completely bypassed for
`arrayfun`; the iterated input is returned verbatim. The same lambda
applied via `cellfun` or `structfun` works correctly. Bug is specific
to the `arrayfun` adapter.
**Impact:** Any MATLAB code using `arrayfun(@(...)..., ...)` produces
silent wrong results — no crash, just wrong answers.
**Where:** `arrayfun` adapter only. The bug is likely in the
arrayfun-specific argument-binding path; cellfun/structfun must use
a different (working) lambda-application code path.
**Status:** **pending fix.** May be libs-side after all (since
cellfun/structfun work) — not necessarily core.
**First seen:** 2026-05-03, parity bulk-bench iteration 7 (probing
`nchoosek` MISMATCH). Refined iter 26 — confirmed cellfun/structfun
unaffected.
**Fixed in commit (next)** — libs/builtin/src/library.cpp arrayfun
adapter was a stub returning args[1] verbatim. Replaced with the
real implementation: walk inputs, callFunctionHandle per element,
pack into a numeric array (UniformOutput=true) or cell
(UniformOutput=false). Multiple input arrays accepted. e2e
`arrayfun-bug11.spec.js` (4 cases).

---

## 12. `libs/builtin`: `polyder` doesn't strip leading zeros from result — **P3** ✅ FIXED

**Reproducer:**
```matlab
p = sin(linspace(0, 5, 100));   % p(1) = 0
y = polyder(p);
% numkit: length(y) = 99   (keeps leading 0 from (N-1)*p(1) = 99*0 = 0)
% MATLAB: length(y) = 98   (strips leading zeros)
```
Values otherwise match — the spec's element-wise SAVE block flags
"length mismatch: 99 vs 98".
**MATLAB:** drops leading zeros from polynomial coefficient vectors after
differentiation (matches its general convention that polynomials are
canonicalized to remove leading-zero terms).
**Impact:** Cosmetic for most uses, but downstream code that depends on
length(polyder(p)) == length(p) - k (with leading zeros stripped) will
diverge.
**Where:** [libs/builtin/src/...polyder...cpp](libs/builtin/) — needs a
post-pass to trim leading zeros until first non-zero coefficient.
**First seen:** 2026-05-03, parity bulk-bench iteration 7.
**Fix (2026-05-03):** Added `trimLeadingZeros(deriv)` in the 1-arg
`polyder` path in [libs/builtin/src/math/poly/polynomials.cpp]. The
helper already existed for the 2-arg form (rational-derivative
cancellation); the single-poly form was just missing the call.

---

## 13. `libs/builtin`: bit-ops reject integer typed arrays — **P2** ✅ FIXED

**Reproducer:**
```matlab
a = uint32(1:5);
bitand(a, a)         % numkit: "Not a double array (in call to 'bitand')"
bitor(a, a)          % same
bitxor(a, a)         % same
bitcmp(a)            % same — and bitcmp NEEDS uint type in MATLAB
% Workaround:
bitand(double(1:5), double(0:4))                % works
bitcmp(double(1:5), 'uint32')                   % works
```
**MATLAB:** all bit-ops accept any integer type (uint8/16/32/64,
int8/16/32/64) AND non-negative double values (the latter as a
historical convenience). numkit only accepts double.
**Impact:** Parity gap. Functionally bit-ops work via the double path,
but typical MATLAB code in the wild stores bitfields as `uint32` /
`uint64` for memory and clarity, and that breaks here.
Specifically, `bitcmp(uint_array)` (the canonical 1-arg form, type
inferred from input) cannot be expressed in numkit at all — must
use the 2-arg `bitcmp(double_array, 'uint32')` form.
**Where:** [libs/builtin/src/](libs/builtin/) bit-op adapters.
**First seen:** 2026-05-03, parity bulk-bench iteration 8.
**Fix (2026-05-03):** Wrapped the bitand/bitor/bitxor/bitshift adapters
in [libs/builtin/src/language/bitwise/int_math.cpp] with a
`runBitwiseBinary` helper that picks the result class via MATLAB's
type rules (both int → same class; mixed-class int → error; one int
+ scalar double → int's class; both double → DOUBLE), then runs the
existing DOUBLE-space pipeline and casts the result back to the
chosen integer class. `bitcmp` got a 1-arg form: integer input infers
the width from its class and returns the same integer class
(`bitcmp(uint8(0))` → `uint8(255)`); double input still requires the
explicit-class 2-arg form.

---

## 14. `core/`: `(:)` on a logical scalar segfaults — **P0** → ✅ FIXED 2026-05-11

**Reproducer:**
```matlab
y = true;
z = y(:);            % Segmentation fault (exit 139)
```
Same crash for `false(:)`, `logical(0)(:)`, `logical(1)(:)`, the
result of `strcmp(eq_strs)`, etc. — anything that produces a scalar
`logical` and is then colon-flattened.
**Works fine:** `scalar_double(:)`, `[true false true](:)` (logical
vector with 2+ elements).
**Symptom:** `Segmentation fault` (Windows: SEH 0xC0000005). No
stderr output, just process death.
**Impact:** Surfaces in any code that flattens a possibly-scalar
boolean for subsequent reduction (`sum(strcmp(a,b)(:))`, common
defensive pattern). Also breaks the parity harness's default
fingerprint `sum(y(:))` for any function whose output may be a
scalar logical (`strcmp`, `contains`, `startsWith`, `isequal`,
`xor`, `any` w/ scalar input, ...).
**Where:** core indexing dispatch — colon flattening on a 1-elem
logical container. Probably misses a special-case for the SBO/scalar
path of logical Value.
**Status:** **FIXED 2026-05-11** (commit pending). Root cause was in
`VM::execIndirectIndex` colon-flatten branch: it called
`mv.rawData()` + `memcpy(n*es)` unconditionally, but tag-stored
scalars (logical SBO, also DOUBLE small-form) have no heap buffer,
so `rawData()` returned nullptr → memcpy garbage / segfault. The
intermediate "no-crash, wrong-value" state was the same issue —
copying from an uninitialised stack location happened to land on a
zero. Fix: identity short-circuit `if (mv.isScalar()) R[I.a] = mv;`
before the memcpy path. Covers every scalar type (LOGICAL / DOUBLE
tag / INT* / STRUCT) without touching raw bytes. Validated via
true(:), false(:), logical(0)(:), logical(1)(:), scalar_double(:),
[true false true](:) — all match MATLAB.
**First seen:** 2026-05-03, parity bulk-bench iteration 10 (probing
`strcmp` SAVE-block hang).

---

## 15. `core/`: `fieldnames(struct)` returns alphabetical, not insertion order — **P2** ✅ FIXED

**Reproducer:**
```matlab
s = struct('alpha',1,'beta',2,'gamma',3,'delta',4,'epsilon',5);
fieldnames(s)
% MATLAB:  {'alpha';'beta';'gamma';'delta';'epsilon'}  (insertion order)
% numkit:  {'alpha';'beta';'delta';'epsilon';'gamma'}  (alphabetical)
```
**MATLAB:** `fieldnames` preserves the order in which fields were
added — this is documented behavior and load-bearing for many MATLAB
codebases that iterate fields with confidence about ordering.
**Impact:** Anything that depends on field insertion order (loops,
printf-style output, JSON serialization) gets reordered silently.
**Where:** `core/` — struct field storage probably uses an ordered
map by name. Should preserve insertion-time slot index.
**First seen:** 2026-05-03, parity bulk-bench iteration 14.
**Fix (2026-05-11):** Added a parallel `std::pmr::vector<std::string> *
fieldOrder` to `HeapObject` (one per struct, shared across array
elements — MATLAB requires a uniform field set), with helpers
`Value::setField` / `setFieldAll` / `removeField` / `fieldNamesInOrder`
in [core/src/value.cpp]. Every mutation site now records the field
slot in insertion order:
* struct array factory + `growStructArrayTo` allocate / copy
  `fieldOrder` ([core/src/value.cpp]);
* TreeWalker's struct-array assignment paths
  ([core/src/tree_walker.cpp]);
* VM `FIELD_SET` broadcast + `STRUCT_ELEM_FIELD_SET`
  ([core/src/vm.cpp]);
* `struct(a,{1,2,3}, ...)` constructor and `setfield` /  `rmfield` in
  [libs/builtin/src/language/structures/struct.cpp].
`fieldnames` now reads `fieldNamesInOrder()` (alphabetical fallback
for legacy structs without a tracker). `orderfields` was a no-op
before (assumed alphabetical = insertion); now it explicitly sorts.
Reproducer covered: `struct('alpha',1,'beta',2,'gamma',3,'delta',4,
'epsilon',5)` → `{alpha;beta;gamma;delta;epsilon}` (MATLAB-exact).
`rmfield` preserves order; `orderfields` returns sorted; struct array
constructor preserves arg order. 165 struct/field-related gtests
pass; full suite shows no struct-related regressions.

---

## 16. `libs/builtin`: `func2str(@sin)` returns `'@sin'` instead of `'sin'` — **P3** ✅ FIXED

**Reproducer:**
```matlab
f = @sin;
func2str(f)
% MATLAB:  'sin'      (3 chars)
% numkit:  '@sin'     (4 chars, includes leading @)
```
**MATLAB:** for a named handle (`@fname`), `func2str` returns just
the function name. The `@` prefix is preserved only for anonymous
handles (`@(x) x*2` → `'@(x)x*2'`).
**Impact:** Cosmetic — anything that displays / serializes function
handles will show a leading `@` that MATLAB omits.
**Where:** [libs/builtin/src/](libs/builtin/) `func2str` implementation —
should branch on "is the handle wrapping a named function?" and
emit name only in that case.
**First seen:** 2026-05-03, parity bulk-bench iteration 14.
**Fixed:** library.cpp's func2str lambda now detects anon-handle naming
convention (`__anon_<N>` prefix) and returns the bare name for named
handles, `@__anon_<N>` placeholder for anonymous (we don't preserve
anon-source-text yet, so this is a best-effort fallback). Func2Str
unit test updated to match MATLAB-spec.

---

## 17. `libs/builtin`: `cellstr(multi_row_char)` flattens column-major instead of row-split — **P2** ✅ FIXED

**Reproducer:**
```matlab
ch = char(['apple   '; 'banana  '; 'cherry  ']);    % 3x8 char matrix
size(ch)         %     [3 8]
y = cellstr(ch);
% MATLAB:  3-element cell: {'apple'; 'banana'; 'cherry'}  (per row, trailing spaces stripped)
% numkit:  1-element cell: {'abcpahpnelarenr ay     '}    (24-char column-major dump)
```
**Symptom:** numkit reads the char matrix in column-major order (Fortran
layout), concatenates ALL elements into one string, wraps in a 1-cell.
The expected behavior is splitting the matrix into N row-strings,
each trimmed of trailing spaces, packaged as N-element cellstr.
**MATLAB:** `cellstr(M)` where M is M-by-N char matrix returns an
M-by-1 cell array with each element being a `deblank`'d row.
**Impact:** Anything that constructs a char block (file I/O, table
formatting) and wants to convert to cellstr produces garbage data.
**Where:** [libs/builtin/src/...cellstr...cpp](libs/builtin/) — needs
to iterate rows of the char matrix and deblank each, not column-major
flatten.
**First seen:** 2026-05-03, parity bulk-bench iteration 15.
**Fix (2026-05-03):** `cellstr` in
[libs/builtin/src/language/cells/cell.cpp] now splits multi-row CHAR
input into M rows (column-major buffer indexing: element (r,c) at
`r + c*nr`), strips trailing spaces from each row (MATLAB's `deblank`
semantics), and packages into an M-by-1 cell. 1-row CHAR continues
to wrap as a 1-by-1 cell.

---

## 18. `libs/builtin`: `cross(A,B)` rejects 3-by-N matrix batch form — **P2** ✅ FIXED

**Reproducer:**
```matlab
a = [1 0 0; 0 1 0; 0 0 1]';   % 3x3, three column-vectors
b = [0 1 0; 0 0 1; 1 0 0]';   % 3x3
cross(a, b)
% MATLAB / Octave: 3x3 matrix of cross products, column-by-column
% numkit:           "cross requires 3-element vectors"
```
**MATLAB:** `cross(A, B)` works on any-shape arrays as long as one
dimension is 3 (the cross-product axis); cross is computed along
that dimension element-wise across the others.
**Impact:** Vectorized 3-vector batch processing (common in graphics,
physics, robotics) requires looping in numkit instead of one call.
**Where:** [libs/builtin/src/](libs/builtin/) `cross` adapter.
**First seen:** 2026-05-03, parity bulk-bench iteration 17.
**Fix (2026-05-03):** `cross` in
[libs/builtin/src/language/arrays/matrix.cpp] now picks the first
dimension of size 3 (matching MATLAB/Octave's dim-selection rule:
nr==3 → cross along rows / batch over columns; otherwise nc==3 →
cross along cols / batch over rows). The 1x3 / 3x1 vector case is
the degenerate 1-batch form. Errors with same-shape mismatch and
"need a dim of length 3" use MATLAB-style messages.

---

## 19. `libs/builtin`: `freqspace(N)` returns wrong-length vector — **P2** ✅ FIXED

**Reproducer:**
```matlab
freqspace(1024)
% MATLAB:  513 elements (= N/2 + 1 for even N), values in [0, 1]
% numkit:  1024 elements,                       values in [-1, ~1)
```
**MATLAB:** `freqspace(N)` returns "frequency spacing for frequency
response" — for even N, that's N/2 + 1 points on `[0, 1]`. (For
N×N 2-D freqspace, it returns differently.) numkit returns N points
on `[-1, 1)`.
**Impact:** Anything calling freqspace with even N gets
2× the elements with wrong starting value.
**Where:** [libs/builtin/src/](libs/builtin/) `freqspace`
implementation — needs to follow MATLAB's docstring formula.
**First seen:** 2026-05-03, parity bulk-bench iteration 17.
**Fixed:** library.cpp's freqspace lambda rewritten with the correct
half-spectrum (even n → n/2+1; odd n → (n+1)/2) and 'whole' form
(n points on [0, 2-2/n]). Bench: parity OK; numkit 11× faster than
MATLAB (sub-millisecond on N=1024). Existing test updated to MATLAB-spec.

---

## 20. `core/`: `shiftdim` doesn't strip trailing singletons — **P2** ✅ FIXED

**Reproducer:**
```matlab
A = ones(1, 1000, 1000);   % size = [1 1000 1000]
B = shiftdim(A);
ndims(B)
% MATLAB:  2          (drops leading singleton, then trailing trailing dim)
% numkit:  3          (only drops the leading singleton, keeps trailing)
```
**MATLAB:** `shiftdim(A)` drops leading singletons AND collapses
trailing singletons until ndims is at most 2 for matrix-shaped data.
**Impact:** Shape-dependent code that expects 2-D output gets 3-D
back (with trailing dim 1, but `ndims` ≠ 2 still confuses callers).
**Where:** core or libs/builtin `shiftdim` — needs to apply MATLAB's
canonical-trailing-dim normalization.
**First seen:** 2026-05-03, parity bulk-bench iteration 17.
**Fix (2026-05-03):** Added trailing-singleton trim to `shiftdimAuto`
in [libs/builtin/src/language/arrays/nd_manip.cpp]. After the
auto-detected leading-singleton shift, trailing dims of 1 are
stripped down to a minimum rank of 2 via `reshapeND`. The explicit
`shiftdim(A, n)` form preserves dims (matches MATLAB).

---

## 21. `libs/builtin`: `meshgrid(xv)` / `interp2(X,Y,...)` 2-D-input forms missing — **P2** ✅ FIXED

**Reproducers:**
```matlab
% Case A: single-arg meshgrid
meshgrid(linspace(0,10,10))
% MATLAB:  equivalent to meshgrid(x, x), returns NxN grid
% numkit:  "meshgrid: requires 2 arguments"

% Case B: 2-D-grid input to interp2
[X, Y] = meshgrid(xv, yv);
interp2(X, Y, V, Xq, Yq)
% MATLAB:  accepts grid-form X/Y as well as vector form
% numkit:  "interp2: X must be a vector"

% Case C: vector-only Xq/Yq treated as 1-D pointwise instead of 2-D grid
interp2(xv, yv, V, xqv, yqv)   % xqv, yqv vectors
% MATLAB:  returns 5x5 grid (implicit meshgrid on Xq/Yq)
% numkit:  returns 1x5 (pointwise pairing only)
```
**MATLAB:** all three forms documented; common patterns in image/2D-data
processing.
**Impact:** Any MATLAB code using the standard `[X,Y]=meshgrid; interp2(X,Y,V,...)`
idiom breaks. Numkit's 1-arg meshgrid + grid-input interp2 are the
default path most snippets use.
**Where:** [libs/builtin/src/](libs/builtin/) `meshgrid` adapter (1-arg
overload) + `interp2` adapter (grid-input + implicit-meshgrid forms).
**First seen:** 2026-05-03, parity bulk-bench iteration 18.
**Fix (2026-05-03, partial):** Case A `meshgrid(xv)` ≡ `meshgrid(xv,xv)`
implemented in [libs/builtin/src/language/arrays/matrix.cpp]
(`meshgrid_reg` adapter handles 1/2/3-arg).

**Fully fixed 2026-05-10.** Case B (matrix Xq/Yq from meshgrid)
worked already; verified via interp2-grid-bug21.spec.js. Case C
(vector Xq/Yq implicit-meshgrid) closed by extending interp2Impl
in libs/builtin/src/math/interp/interp.cpp — when both queries
are vectors, output `length(Yq) × length(Xq)` sampled at every
(Xq[j], Yq[i]) instead of pointwise. Pointwise stays for matrix-
shaped Xq/Yq (the meshgrid output case).

---

## 22. `libs/builtin`: `spline(x, v)` (2-arg pp-struct form) unsupported — **P2** ✅ FIXED

**Reproducer:**
```matlab
pp = spline(linspace(0,10,50), sin(linspace(0,10,50)));
% MATLAB:  returns a piecewise-poly struct usable with ppval
% numkit:  "spline: requires 3 arguments"
```
**MATLAB:** `spline(x, v)` with 2 args returns a `pp` struct (the
spline's piecewise-polynomial form). With 3 args `spline(x, v, xq)`
evaluates at xq directly. numkit only supports the 3-arg form.
**Impact:** Cannot pre-compute a spline once and evaluate via ppval
many times — the canonical "factor + apply" pattern.
**Where:** [libs/builtin/src/](libs/builtin/) `spline` adapter.
**First seen:** 2026-05-03, parity bulk-bench iteration 18.
**Fix (2026-05-03):** Added a `splinePp(x, y)` helper in
[libs/builtin/src/math/interp/interp.cpp] that runs the existing
natural-cubic-spline tridiagonal solve, transforms the second-
derivative form into per-interval `[a*dx^3 + b*dx^2 + c*dx + d]`
coefficients, and packages them via `mkpp` into a pp struct. The
2-arg `spline_reg` form routes here; 3-arg keeps direct evaluation.
Verified: `ppval(spline(x, v), xq)` matches `spline(x, v, xq)` to
~1e-16 on `sin(linspace(0, 10, 50))`.

---

## 23. `libs/builtin`: `meshgrid(x,y,z)` 3-arg form returns only 2 outputs — **P2** ✅ FIXED

**Reproducer:**
```matlab
[X, Y, Z] = meshgrid(linspace(-2,2,5), linspace(-2,2,5), linspace(-2,2,5));
% MATLAB:  X, Y, Z each 5x5x5
% numkit:  X is 5x5 (2-D!), Y returned, Z undefined
```
**Symptom:** numkit's meshgrid stops at 2-D regardless of how many
input vectors / output slots are given. Cannot construct 3-D grids
for volumetric processing or `cart2sph(X,Y,Z)` / `sph2cart(...)`.
**MATLAB:** `meshgrid(x,y,z)` returns three M×N×P arrays (M=length(y),
N=length(x), P=length(z)).
**Impact:** Anything using 3-D grids fails — cart2sph, sph2cart with
ndgrid-input, volumetric interpolation (interp3 uses meshgrid'd
inputs internally in some idioms), 3-D plotting setup.
**Where:** [libs/builtin/src/](libs/builtin/) `meshgrid` adapter — only
implements 2-arg / 2-output form. Already partially flagged in
BUGS #21 (1-arg form missing); add the 3-arg form too.
**First seen:** 2026-05-03, parity bulk-bench iteration 19.
**Fix (2026-05-03):** Added a 3-input `meshgrid(x, y, z)` overload in
[libs/builtin/src/language/arrays/matrix.cpp] returning three
`[ny, nx, nz]` 3-D DOUBLE arrays. Adapter dispatches by `args.size()`:
1-arg (BUGS #21 case A), 2-arg, 3-arg.

---

## 24. `core/`: `.*` (and other elementwise ops) reject `logical .* double` — **P2** ✅ FIXED

**Reproducer:**
```matlab
x = (mod(1:10, 3) == 0) .* (1:10)
% MATLAB / Octave:  [0 0 3 0 0 6 0 0 9 0]   (auto-coerce logical → double)
% numkit:           "Unsupported types for .* (in operator '.*')"
```
**MATLAB:** logical values auto-coerce to double in arithmetic ops.
The pattern `(predicate) .* values` is canonical for masking.
**Impact:** Defensive masking idioms break; users must explicitly
`double(predicate) .* values`. Possibly affects `+`, `-`, `./` too.
**Where:** core elementwise dispatch — needs to insert auto-cast for
logical operands when paired with numeric.
**First seen:** 2026-05-03, parity bulk-bench iteration 20.
**Fix (2026-05-03):** Added a `coerceLogicalToDouble` helper at the
top of [libs/builtin/src/language/operators/binary_ops.cpp]: any
arithmetic op (plus / minus / times / mtimes / rdivide / mrdivide /
mldivide / power / elementPower) that sees a LOGICAL operand
recurses with both operands cast to DOUBLE. Bool-mask idioms like
`(mask) .* values` and `predicate + array` now work without explicit
`double(...)` wrap.

---

## 25. `libs/builtin`: `isStringScalar` (camelCase) not registered — **P3** ✅ FIXED

**Reproducer:**
```matlab
s = "hello";
isStringScalar(s)   % MATLAB: 1   numkit: undefined function
isstringscalar(s)   % numkit:  1   MATLAB: undefined function
```
**MATLAB:** the canonical name is camelCase (`isStringScalar`). numkit
only registers the lowercase alias.
**Impact:** Cross-MATLAB code that uses the canonical camelCase name
fails. Lowercase already works.
**Where:** [libs/builtin/src/](libs/builtin/) — add `isStringScalar`
as alias to existing `isstringscalar` registration. Cosmetic / 1-line
fix.
**First seen:** 2026-05-03, parity bulk-bench iteration 20.
**Fixed:** added camelCase alias to library.cpp registration. Both
names now resolve to the same impl.

---

## 26. `libs/builtin`: `num2str(X, fmt)` and `num2str(X, n)` ignore the precision argument — **P2** ✅ FIXED

**Reproducer:**
```matlab
x = pi;
num2str(x)             % numkit: '3.14159'  MATLAB: '3.1416'  (defaults differ — both reasonable)
num2str(x, '%.10f')    % numkit: '3.14159'  MATLAB: '3.1415926536'
num2str(x, 12)         % numkit: '3.14159'  MATLAB: '3.14159265359'
```
**Symptom:** numkit returns the same 7-digit default regardless of
the second argument — both the printf format-string form and the
digit-count form are silently ignored.
**MATLAB:** `num2str(X, fmt)` honors the C-style printf spec, and
`num2str(X, n)` returns up to n significant digits.
**Impact:** Anything formatting numerics for display, logging, or
serialization gets stuck at numkit's default precision. Common usage.
**Where:** [libs/builtin/src/](libs/builtin/) `num2str` adapter — needs
to actually parse + apply the second arg, branching on its type.
**First seen:** 2026-05-03, parity bulk-bench iteration 21.
**Fixed:** added 2-arg overload to `num2str(mr, x, spec)` in
strings.cpp (and matching declaration in strings.hpp + reg in
strings.cpp). Spec branches on type: char/string → snprintf with the
fmt; numeric → `%.<N>g` format. Default 1-arg form changed from
ostringstream-default-6 to `%.5g` (matches MATLAB's documented
5-significant-digit default).

---

## 27. `libs/builtin`: `*Between` family handles only first occurrence — **P2** ✅ FIXED

**Reproducers:**
```matlab
s = '<<a>><<b>><<c>>';
extractBetween(s, '<<', '>>')
% MATLAB:  3-element cell {'a';'b';'c'}
% numkit:  single char 'a' (just first match)

eraseBetween(s, '<<', '>>')
% MATLAB:  '<<>><<>><<>>' (all 3 replaced)
% numkit:  '<<>><<b>><<c>>' (only first)

replaceBetween(s, '<<', '>>', 'X')
% MATLAB:  '<<X>><<X>><<X>>'
% numkit:  '<<X>><<b>><<c>>'
```
**Symptom:** numkit's `extractBetween` / `eraseBetween` / `replaceBetween`
all stop after the first matched delimiter pair. MATLAB iterates all
non-overlapping matches.
**Impact:** Multi-match string processing breaks. Common pattern for
log parsing, template extraction, etc.
**Where:** [libs/builtin/src/](libs/builtin/) the three adapters — likely
share an internal "find next pair" helper that needs a `while` loop
instead of a single `find`.
**First seen:** 2026-05-03, parity bulk-bench iteration 23.
**Fix (2026-05-03):** Added `findAllBetweenPairs` helper in
[libs/builtin/src/language/strings/strings.cpp]: walks left-to-right,
collecting every non-overlapping (open, close) delimiter pair (advance
cursor past the closing delim each iteration). `extractBetween` now
always returns an Mx1 cell of inner strings (matching MATLAB —
including for single matches); `eraseBetween` / `replaceBetween` walk
the match list and rebuild the string in one pass. Numeric-anchor
inputs still match at most once. Existing test updated to MATLAB-spec.

---

## 28. `core/`: `mldivide` / `mrdivide` / `mpower` named-fn forms not implemented — **P2** ✅ FIXED

**Reproducers:**
```matlab
A = eye(3) + 0.01*[1 2 3; 4 5 6; 7 8 9];
B = [1; 2; 3];
mldivide(A, B)
% MATLAB / Octave:  3-element solution vector
% numkit:           "Matrix left division not implemented"

mrdivide(A, B')   % same — "not implemented"
mpower(A, 2)      % same
```
**Symptom:** numkit ✅-marks these in PROGRESS as "named-fn form
added in Pack 11" but the actual function body throws "not implemented".
The `\` and `/` and `^` operator dispatch may work; only the named-fn
adapters are stubs.
**MATLAB:** these are core matrix solve / inverse-matmul / matrix-power
ops; named-fn form is just `mldivide(A,B) === A\B`.
**Impact:** Code that uses `mldivide(A, B)` style (cleaner for
generic-programming) breaks. Workaround = use operator form.
**Where:** [libs/](libs/) — likely the dispatch table maps the named
form to a not-yet-wired-up routine. Linalg as a whole is deferred,
but these named-fn stubs are misleading because the row says ✅.
**First seen:** 2026-05-03, parity bulk-bench iteration 24.
**Fixed in commit (next)** — mldivide / mrdivide were already
wired correctly (`A \ b` and `b / A` produced real results). The
remaining gap was `mpower(A, n)` for matrix A: the underlying
`power()` op in libs/builtin/src/language/operators/binary_ops.cpp
fell through to "Matrix power not implemented". Now when a is a
square 2-D numeric matrix and b is a non-negative integer scalar,
the operator computes the matrix-product chain A·A·…·A via
`mtimes`; n=0 returns the identity matrix of matching size. Non-
integer exponents and inverse / fractional powers (eigendecomp
route) remain BACKLOG. e2e mldivide-bug28.spec.js pins all three
named-fn forms.

---

## 29. `libs/builtin`: `idivide(int_array, ...)` rejects integer-typed input — **P2** ✅ FIXED

**Reproducer:**
```matlab
idivide(int32([1 2 3 4 5]), int32(2))
% MATLAB / Octave:  int32([0 1 1 2 2])
% numkit:           "Not a double array (in call to 'idivide')"
```
**MATLAB:** `idivide` is documented as integer-only (it converts
fractional results to integers via specified rounding mode). It
specifically REQUIRES integer-typed input — `idivide(double, ...)`
errors in MATLAB.
**Impact:** numkit's behavior is the inverse — it requires double
and rejects int. Same root cause class as BUGS #13 (bit-ops reject
int input).
**Where:** [libs/builtin/src/](libs/builtin/) `idivide` adapter.
**First seen:** 2026-05-03, parity bulk-bench iteration 24.
**Fix (2026-05-03):** Rewrote `idivide` in
[libs/builtin/src/library.cpp](libs/builtin/src/library.cpp) to
match MATLAB semantics: require ≥1 integer operand, accept the
other as same-class integer or scalar double; reject double-double
and mixed-class integer inputs with MATLAB-exact error messages.
Internally converts both to DOUBLE for divide+round, then casts back
to the integer operand's class via `builtin::cast`. Result type
matches the integer operand (preserves int8/16/32/64 and uint8/…).

---

## 30. `libs/builtin`: `true(M, N)` / `false(M, N)` shape-form rejected — **P2** ✅ FIXED

**Reproducer:**
```matlab
true(100, 100)
% MATLAB: 100x100 logical array, all true
% numkit: "Index exceeds array dimensions (in function call)"
```
Same for `false(M, N)`. The 0-arg `true` / `false` literals work
fine; only the size-arg call form is broken.
**MATLAB:** `true` and `false` are dual-mode — used as a literal
(`x = true`) and as a logical-array constructor (`true(M, N)`),
parallel to `zeros` / `ones`.
**Impact:** Anything constructing logical arrays via the canonical
`true(M,N)` idiom fails; users must `logical(ones(M,N))` instead.
**Where:** [libs/builtin/src/](libs/builtin/) — true/false adapters
need the size-arg overload (mirror `ones` adapter).
**First seen:** 2026-05-03, parity bulk-bench iteration 27.
**Fix (2026-05-03):** `true`/`false` were lexer keywords producing
`BOOL_LITERAL` AST nodes, bypassing function dispatch entirely. Made
them MATLAB-style built-in functions: removed from lexer keyword
table (now emit `IDENTIFIER`), removed from `kBuiltinConstants` and
`constantsEnv_` (so VM compiler routes them through function lookup
instead of preloading from constants), added `true_reg` / `false_reg`
in `libs/builtin/src/language/arrays/matrix.cpp` mirroring `ones_reg`
shape parsing. Bare `true`/`false` resolve to scalar logicals via
0-arg call; `true(M, N, ...)` builds an MxN... logical array.

---

## 31. `libs/builtin`: `interpn` (N-D interpolation) not implemented — **P2** ⚠ partial

**Reproducer:**
```matlab
[X, Y, Z] = ndgrid(linspace(0,10,20));   % already fails — see BUG #23
% Even with workaround, interpn(...) returns nothing usable.
```
**MATLAB:** `interpn(X1, X2, ..., V, Xq1, Xq2, ...)` is N-D linear
interpolation. Marked ✅ in PROGRESS but the function call
fails (probably tied to BUGS #23 — meshgrid 3-arg missing — and
the underlying interp infrastructure not generalized to N-D).
**Where:** [libs/builtin/src/](libs/builtin/) `interpn` adapter.
Likely needs 3-D meshgrid first.
**First seen:** 2026-05-03, parity bulk-bench iteration 27.
**Partial fix (2026-05-10):** Verified `interpn` dispatches correctly
to interp2 / interp3 for ndim ∈ {2, 3}. e2e `interpn-bug31.spec.js`
pins both paths (2-D bilinear + 3-D trilinear). True 4+-D
interpolation (generic ND tensor-product linear interp over 2^N
corners per query) remains BACKLOG.

---

## 35. `libs/signal`: `chebwin(N, R)` degenerates to all-ones for large N — **P2** ✅ FIXED

**Reproducer:**
```matlab
chebwin(10, 100)
% MATLAB:  Dolph-Chebyshev window — values in [0, 1] with main-lobe shape
% numkit:  small-N case roughly works (some structure)
chebwin(1024, 100)
% MATLAB:  proper window, sum ~378
% numkit:  all 1s, sum = 1024
```
**Symptom:** numkit's `chebwin` returned the correct shape for very small
N but degenerated to all-ones (or all-near-1) for typical analysis
sizes (N ≥ ~64). Root cause was a half-bin offset in the FFT-based
inverse-cosine spectrum reconstruction — for even N the k = N/2
nyquist term landed on the wrong centre of symmetry and the time-
domain coefficients summed to a constant.
**MATLAB:** all N values produce the proper window with main-lobe-to-
sidelobe ratio R dB.
**Impact:** Anything using `chebwin` for spectral analysis at typical
sizes got a rectangular (no-window) result silently — wrong leakage
properties.
**Where:** [libs/signal/src/windows/windows.cpp](libs/signal/src/windows/windows.cpp) `chebwin`.
**First seen:** 2026-05-03, parity bulk-bench iteration 33.
**Fix (2026-05-08):** Replaced FFT-based reconstruction with the direct
O(N²) cosine-IDFT form. Spectrum samples W(k) = T_M(β·cos(πk/N)) for
k = 0..floor(N/2), reconstructed in time domain via the real cosine
basis centred on N₀ = (N-1)/2. Verified `chebwin(1024, 100)` now
returns sum ≈ 379 (matches MATLAB ~378), proper peak-1 / taper-to-
zero shape, exact symmetry. Smoke `libs/signal/tests/smoke/chebwin_large_smoke.m`
locks the regression (N=128/256/512/1024/2048 all produce correct
windows).

---

## 32. `libs/signal`: `impzlength` overestimates IIR impulse-response length — **P3** ✅ FIXED

**Reproducer:**
```matlab
b = [1 -0.5]; a = [1 -0.99];
impzlength(b, a)
% MATLAB:  985
% numkit:  1146
```
Both engines return a length sufficient for the response to decay below
threshold; numkit's estimate is conservative (~16% longer).
**Impact:** Cosmetic — anything that allocates buffers based on this
gets a slightly larger array. Functionally correct.
**Where:** [libs/signal/src/](libs/signal/) — different decay-tolerance
constant or different pole-magnitude formula.
**First seen:** 2026-05-03, parity bulk-bench iteration 29.
**Fixed in commit 67e8a8a1** (2026-05-09). Root cause: numkit had a
min-cap at 50 + a different decay tolerance. Fix used MATLAB's
canonical `floor(log(5e-5) / log(rho))` formula with no minimum cap
(other than 1). Bit-identical with MATLAB on rho ∈ {0.1, 0.5, 0.7,
0.9, 0.99} → {4, 14, 27, 93, 985}. Regression guard added in
ide/desktop/tests/e2e/impzlength-bug32.spec.js (2026-05-10) — pins
the canonical 985 / FIR-trivial / pole-magnitude scaling values.

---

## 33. `libs/signal`: `cconv(a, b)` defaults to N=length(a), MATLAB defaults to length(a)+length(b)-1 — **P2** ✅ FIXED

**Reproducer:**
```matlab
cconv(rand(1,1000), rand(1,1000))
% MATLAB:  1999-element vector (length(a) + length(b) - 1, linear conv length)
% numkit:  1000-element vector (length(a) only)
```
**MATLAB:** the documented default for `cconv(a, b)` is N = length(a) +
length(b) - 1, equivalent to a linear convolution. The 3-arg form
`cconv(a, b, N)` does true circular convolution with period N.
**Impact:** numkit's `cconv(a, b)` (2-arg form) returns the
length-N=length(a) circular conv, which is the WRONG default per
MATLAB semantics. Real parity bug.
**Where:** [libs/signal/src/](libs/signal/) `cconv` — fix default N.
**First seen:** 2026-05-03, parity bulk-bench iteration 29.
**Fixed:** convolution/extras.cpp's cconv now uses N = nx + ny - 1
when no N supplied (matches MATLAB linear-conv length). Bench
correctness now OK; perf is 47× slower than MATLAB at N=1999 because
of the O(N²) inner loop — MATLAB uses FFT-based circular conv.
Perf-only follow-up if needed (likely composes via FFT in libs/signal/transforms).

---

## 34. `libs/signal`: `convmtx` element-ordering / shape differs — **P2** ✅ FIXED

**Reproducer:**
```matlab
h = [1 -0.5 0.25];
A = convmtx(h, 100);
A(1, 2)
% MATLAB:  0       (lower-triangular Toeplitz layout, [1 0 0 ...; -0.5 1 0 ...; ...])
% numkit:  -0.5    (different shape — possibly upper-triangular or transposed)
```
The fingerprint sums match (both produce a 102×100 matrix with the same
sum), but the element layout differs.
**Impact:** Anything using `convmtx` for matrix-formed convolution
(linear regression formulation, etc.) gets the wrong product.
**Where:** [libs/signal/src/](libs/signal/) `convmtx` — likely shape /
indexing convention bug.
**First seen:** 2026-05-03, parity bulk-bench iteration 29.
**Fix (2026-05-03):** `convmtx` in
[libs/signal/src/convolution/extras.cpp] now branches on h's shape:
* Row h (or 1-D / scalar) → returns `n × (n + nh - 1)`, row k holds
  h shifted right by k, so `x_row * M == conv(x, h)`.
* Column h (rows > 1) → keeps the previous `(n + nh - 1) × n` form,
  column k = h shifted down by k, so `M * x_col == conv(h, x)`.
Verified both forms against MATLAB R2025b. Existing tests updated
to assert the MATLAB-shape conventions (was numkit-bug shape).

---

## 36. WASM: Bessel family throws at runtime — `std::cyl_bessel_*` not in libc++ — **P2** ✅ FIXED

**Functions:** `besselj`, `bessely`, `besseli`, `besselk`, `besselh`,
`airy` (Airy via Bessel), `ellipke` (uses Bessel internally) — every
caller of `std::cyl_bessel_j / i / k` and `std::cyl_neumann` in
[libs/builtin/src/math/special/special.cpp](libs/builtin/src/math/special/special.cpp).

**Symptom:** On the WASM browser build only, calling any Bessel-family
function threw `Error("besselj: Bessel family not yet supported in
the WASM build…")`. Desktop build (MSVC / libstdc++) worked correctly.

**Root cause:** C++17 special math (P0226) is an **optional** part of
`<cmath>`. Microsoft STL and libstdc++ implement it; **libc++ (used by
Emscripten) does not**. The functions were added in commit `78f63d0`
("special: add Bessel family via C++17 std::cyl_*") which compiled and
tested only on MSVC desktop, breaking the WASM build silently until
the first WASM rebuild attempt months later.

**Fix (2026-05-11):** Portable shim in `bessel_portable` namespace,
verified against `std::cyl_*` to 1e-12..1e-15 across the full
test grid (machine-epsilon for I_ν, ~1e-13 for J/Y/K_ν reflection).
Three layers:

1. **Integer order J / Y** — POSIX `jn(int, double)` /
   `yn(int, double)` from emscripten's libm.
2. **Integer order I / K** — power series for |x| ≤ 9 (K) / 20 (I),
   asymptotic Hankel expansion for larger x, K_1 from the I-K
   Wronskian, K_n by forward recurrence (stable for K).
3. **Fractional order any J/Y/I/K** — direct power series for J_ν
   and I_ν (any real ν via Γ(ν+1)); Y_ν and K_ν via the standard
   reflection formula (sin(νπ) ≠ 0 by definition for non-integer ν).
   Negative integer orders dispatch to |ν| with parity flip
   (J_{−n} = (−1)^n J_n; I_{−n} = I_n; etc.).

Smoke `libs/builtin/tests/smoke/bessel_integer_order_smoke.m` locks
J/Y/I/K(n=0..3, x ∈ {0.5, 1, 5, 10}) plus large-x asymptotic
spot-checks; fractional ν coverage is regression-tested through
existing `airy()` / `ellipke()` engine tests once WASM is
redeployed. Same script run on desktop and WASM produces identical
numbers. Asymptotic for fractional ν at very large x (>~25) and
non-positive-integer ν reflection edge cases remain BACKLOG (not
hit by typical MATLAB scripts).

---

## 37. `libs/wavelet`: `wavedec`/`dwt` produce non-MATLAB coefficients on db/sym/coif — **P2**

**Reproducer:**
```matlab
x = 1:16;
[c, l] = wavedec(x, 3, 'db2');
disp(c(1:4))           % cA_3
% MATLAB:  3.883280   3.625881  21.410324  42.756277
% numkit:  8.093341  32.790118  44.527383  42.450090
```
Round-trip `waverec(c, l, 'db2')` recovers `x` to ≤1e-11 in both
engines, so numkit's transform is internally consistent — but the
coefficients themselves differ from MATLAB (different boundary /
downsampling offset convention in the underlying `dwt`).

**Impact:** Anything that exposes the (c, l) pair to user code — or
that compares numkit coefficients to MATLAB — breaks parity for db /
sym / coif wavelets. **Haar matches** (boundary handling is trivial
for filter length 2). Functions affected: `wavedec`, `dwt`, `appcoef`,
`detcoef`, `wrcoef`, `wdenoise`, anything that relies on coefficient
values.

**Where:** [libs/wavelet/src/dwt/dwt.cpp](libs/wavelet/src/dwt/dwt.cpp)
— the symmetric-extension boundary path. Specifically the
"`cA[k] = y[Lf + 2k]`" downsampling rule (lines 112-126) chooses a
different output-keep offset than MATLAB's `dwt`. Round-trip works
because `idwt` mirrors the choice exactly, but it breaks one-sided
parity with MATLAB.

**Fix sketch:** rebase the dwt boundary handling on MATLAB's exact
convention. The MATLAB doc points to "periodised" or "symmetric whole-
point" with start offset depending on filter length parity; verifying
against MATLAB's documented `dwt('db2', x)` output for several short
inputs would pin down the offset rule. Then idwt mirrors.

**Discovered:** 2026-05-04 while implementing `wrcoef` (commit
`184f04a` HEAD). wrcoef parity is therefore verified only against
MATLAB on `'haar'`; on `db2` the spec checks the round-trip identity
`a3 + sum(d_i) == x` (which numkit satisfies internally) instead of
direct value comparison.

---

## 38. `ide/`: `plot()` linespec/N-V parameters partially ignored by renderer — **P1** ✅ FIXED

**Symptom:** in
```matlab
plot(x, sin(x),         'b-',   'LineWidth', 2)
plot(x, sin(x - pi/4),  'r--o', 'LineWidth', 1.5, 'MarkerSize', 4)
plot(x, sin(x - pi/2),  'g:s',                    'MarkerSize', 5)
```
all three lines render as **solid, default-thickness, marker-less**
strokes whose only visible difference is colour.

**Diagnosis:** kernel emits the right JSON — the figure stream from
`numkit_example.exe` shows
`{"style":"r--o","lineWidth":1.5,"markerSize":4}` etc. Loss happens in
the IDE rendering pipeline, in three independent places:

1. **`adapters.js` `parseLineSpec`** — only extracts `color`. Never
   reads the dash hint (`-`, `--`, `:`, `-.`) or the marker glyph
   (`o`, `s`, `*`, `^`, `+`, `x`, `d`, `v`, `<`, `>`, `p`, `h`). So
   `'r--o'` reduces to `{color: '#f07070'}` — same as `'r'`.

2. **`CompositePlot.jsx` line render (line ≈1176)** — emits
   `<path stroke={ly.color} strokeWidth={w} ... />` with no
   `strokeDasharray` switch on `ly.lineStyle`. Even if (1) were fixed,
   the path would still draw solid.

3. **`CompositePlot.jsx` line render** — never overlays markers on a
   line layer. Markers only render under `mode === 'scatter'`. So
   `plot(x, y, 'r-o')` (line + circle markers) loses the markers
   entirely; only `scatter(x, y)` shows them.

4. **`LineWidth` chain proper** — verified end-to-end OK for `mode ===
   'line'` (kernel → `d.lineWidth` → adapter `width:` → `w =
   ly.width || 1.5` → `<path strokeWidth={w}>`). The user-facing
   "params not applied" perception comes from (1)+(2)+(3) hiding the
   difference between `'b-'` LW=2 and `'g:s'` LW-default; once
   markers + dashes are restored the LW change becomes visible.

**Test gap:** no e2e covers any linespec/N-V param. `grep -rn
'LineWidth\|MarkerSize\|linewidth' ide/desktop/tests/e2e/` → empty.

**Plan when fixing:**
- Extend `parseLineSpec` to emit `{color, lineStyle: '-' | '--' | ':' |
  '-.', marker: 'o'|'s'|'*'|'^'|...}` based on a tokeniser that walks
  the spec left-to-right (longest-match for dashes, then the marker
  glyph). MATLAB-compatible token table is small.
- Adapter: forward `lineStyle` and `marker` onto the layer, separate
  from `style` string. Don't break heatmap/scatter etc.
- `CompositePlot` `mode === 'line'` branch: pick `strokeDasharray`
  from a 4-entry table; if `ly.marker` set, draw an overlay `<g>` of
  marker shapes after the path.
- Add e2e `linespec-params.spec.js` covering: solid/dashed/dotted/
  dash-dot widths from kernel; marker glyph variations; LineWidth=N;
  MarkerSize=M.

**Severity P1:** wrong visual output of one of the most common MATLAB
calls. Not a crash and color does propagate, so users see a plot — but
visual diffs that the script asks for silently disappear.

**First seen:** 2026-05-10, reported during imshow design discussion.

**Fixed in commit 6c6a4fa0** (2026-05-10). New e2e
`linespec-params.spec.js` (8 cases) covers LineWidth, MarkerSize,
dashed/dotted lineStyles, and the marker glyph dispatcher.

---

## 39. `ide/`: 3-D scene — grid toggle resets camera, grid sits on front faces — **P1** ✅ FIXED

Two related defects in `Composite3DPlot.jsx`. Filed together because
both touch the per-figure rebuild effect and the back-face grid.

### 39a. Toggling `grid` resets the OrbitControls camera

**Repro:** open a 3-D figure (e.g. `surf(peaks)`), drag-orbit to a
custom angle, then click the toolbar `grid` button.
**Expected:** the grid lines appear / disappear, camera stays put.
**Actual:** camera snaps back to `figure.view` (or to the default
`(-37.5°, 30°)` when `view` was never set), losing the user's orbit.

**Root cause:** `Composite3DPlot.jsx:1090` is one mega-effect deps'd
on `[figure, fontScale, viewport3d, effectiveMajor, effectiveMinor]`.
Inside, after rebuilding the axes frame, the code unconditionally
re-applies `figure.view`:
```jsx
if (Array.isArray(figure.view) && figure.view.length === 2) {
  const off = azElToCameraOffset(figure.view[0], figure.view[1], 4);
  c.camera.position.set(off.x, off.y, off.z);   // ← clobbers orbit
  c.camera.lookAt(0, 0, 0);
  c.controls.update();
}
```
A grid toggle bumps `effectiveMajor` / `effectiveMinor` → the effect
re-runs → camera teleports.

**Fix plan:**
- Split the effect: (a) data + axes (deps `[figure, viewport3d]`); (b)
  grid-only update on `[effectiveMajor, effectiveMinor]` that just
  toggles `.visible` on a pre-built `gridMajorGroup` / `gridMinorGroup`.
- Apply `figure.view` only when it actually changed (compare against a
  `lastViewRef`), not on every rebuild.
- Add e2e: orbit camera (drag), toggle grid, assert
  `camera.position` after equals before.

### 39b. Grid lines render on front faces, hiding the surface

**Repro:** any `surf` / `mesh` / `bar3` figure with grid on. Orbit so
that the data-Y-positive face (or X-min, Z-min) faces the camera.
**Expected:** grid lines stay on the **three back faces** (MATLAB
behaviour — grid is a backdrop, never overdraws the data).
**Actual:** grid lines stay on the same three world faces forever:
`y = -1`, `z = -1`, `x = -1`. Those faces were chosen for the
default camera azimuth (-37.5°), so any orbit past ±90° in az or
flipping el moves the grid in front of the surface — see screenshot
in BUGS attachments / chat 2026-05-10.

**Root cause:** `buildAxesFrame` (`Composite3DPlot.jsx:735-795`) emits
six fixed `LineSegments` groups on three hard-coded faces. There's
no per-frame face-selection. The comment at line 739 already calls
this out:
```js
// Without knowing camera azimuth up-front we draw the grid on the
// faces opposite the default camera. Cheap and good enough; a
// follow-up will reposition them per-frame.
```

**Fix plan (cheap version — toggling, not rebuilding):**
- Build SIX groups: `gridXminus / gridXplus / gridYminus / gridYplus
  / gridZminus / gridZplus` (and minor counterparts). All added to
  the scene at build time.
- Per-frame in `tick()`: project `camera.position` onto each face's
  outward normal. Three faces with the **largest negative** projection
  are the back faces; flip `.visible` accordingly. Cost: 6 dot
  products per frame, negligible.
- Same logic for tick labels — back-face labels are also stuck on
  fixed walls today.
- e2e: build a 3-D figure, orbit to a non-default angle (programmatic
  via setView), assert that the visible grid groups changed (count
  the visible `LineSegments` in the right group).

**Severity P1:** affects every 3-D plot the moment the user touches
the camera. (39a) breaks every grid toggle. (39b) is the "MATLAB
visual-quality" bar that we explicitly chose to clear in the cycle
that landed WebGL 3-D.

**First seen:** 2026-05-10, reported with a `surf`-style screenshot
showing grid on top of the peaks surface.

**Fixed in commit 0f8a42d9** (2026-05-10). New e2e
`3d-grid-camera.spec.js` (4 cases) + `canvas.__numkit3dCtx`
test-inspection hook covering camera stability, six-face grid
construction, and per-frame visibility flips on orbit.

---

## 40. `libs/builtin`: matrix `A^k` doesn't throw on non-square / bad shape — **P2**

**Test:** `ArithBatchTest.MatrixPowerThrows`
**File:** [libs/builtin/tests/arith_batch_test.cpp:81](libs/builtin/tests/arith_batch_test.cpp:81)
**Symptom:** `Value of: threw` is `false`. The test expected `^`
applied to a matrix whose shape disallows the operation (rectangular
or other invalid combination) to throw; numkit returns a result
silently.
**Reproducer:** see test body around line 81 — feeds an invalid shape
to `^` / `mpower`.
**Impact:** Wrong result instead of an error; silent corruption.
**Status:** pre-existing; not caused by 2026-05-13 API/Doxygen sweep
(test failed on baseline too, no impl files in the relevant TU were
touched).
**First seen:** present at 2026-05-13 baseline.

---

## 41. `libs/builtin`: `interp2` array-query path wrong + missing shape-mismatch error — **P1**

**Tests:**
- `InterpTest.Interp2ArrayQuery`
- `InterpTest.Interp2QueryShapeMismatchThrows`

**File:** [libs/builtin/tests/interp_test.cpp:329,359](libs/builtin/tests/interp_test.cpp:329)

**Symptom 1 (Interp2ArrayQuery):** With array query coordinates
`interp2(V, Xq, Yq)` returns wrong values. Test expectations are
`y(2)=0.25`, `y(3)=1.0`; we return `0.5` and `2.0`.

**Symptom 2 (Interp2QueryShapeMismatchThrows):** With mismatched
query-coord shapes `interp2(V, [1 1.5], [1 1.5 2])` should throw;
numkit returns a result silently.

**Impact:** Wrong numeric results on bilinear interp + missing input
validation. Common pattern in image-resampling code.
**Status:** pre-existing.
**First seen:** present at 2026-05-13 baseline.

---

## 42. `libs/stats`: `quantile` linear interpolation uses wrong formula — **P1**

**Tests:**
- `TW_VM/StatsTest.QuantileLinearInterp/{TW,VM}`
- `TW_VM/StatsTest.QuantileVectorPMatrixDim2/{TW,VM}`

**File:** [libs/builtin/tests/stats_test.cpp:222,267](libs/builtin/tests/stats_test.cpp:222)

**Symptom:**
- `quantile([1 2 3 4 5], 0.25)` → `1.75` (MATLAB: **2.0**)
- `quantile([1 2 3 4 5], 0.10)` → `1.0`  (MATLAB: **1.4**)
- Matrix-form along dim 2 mismatches by a fixed 0.25 / 2.5 offset
  across every output entry — consistent off-by-one in the
  position-to-quantile mapping.

**MATLAB:** uses `p_i = (i - 0.5) / N` for the data points (Type-5).
Our formula appears to be Type-7 (`p_i = (i - 1) / (N - 1)`).

**Impact:** Statistical quantiles wrong everywhere — `iqr`, `prctile`,
percentile-based estimators all downstream.
**Status:** pre-existing; correlated with bug #42a.
**First seen:** present at 2026-05-13 baseline.

---

## 42a. `libs/stats`: `iqr` default off — propagation of #42 — **P1**

**Test:** `MovingTest.IqrUniformVector`
**File:** [libs/stats/tests/moving_extras_test.cpp:167](libs/stats/tests/moving_extras_test.cpp:167)
**Symptom:** `iqr(1:9)` → `4.5` (MATLAB: **4.0**). Q3 − Q1 with the
Type-7 formula from bug #42 yields `7 − 2.5 = 4.5`; Type-5 (MATLAB)
yields `7 − 3 = 4`. Same root cause — fixing #42 fixes this.
**Status:** pre-existing.
**First seen:** present at 2026-05-13 baseline.

---

## 43. `libs/graphics`: `imshow` doesn't tag RGB image type on the resulting handle — **P2**

**Test:** `ImshowTest.RGBPathSetsImageRgbType`
**File:** [libs/graphics/tests/figure_test.cpp:1374](libs/graphics/tests/figure_test.cpp:1374)
**Symptom:** After `imshow(rgbImage)` the resulting image handle
lacks the expected `rgb` type tag (or `CData` shape signalling RGB
mode). Equality assertion fails.
**Impact:** Downstream consumers checking `h.Type` / `image.CData`
size to branch on RGB-vs-grayscale don't see the right tag; mostly
affects test introspection + property-driven re-render logic.
**Status:** pre-existing.
**First seen:** present at 2026-05-13 baseline.

---

## 44. `core/`: VM destructure-then-reference shadows local with single-letter / short names — **P1**

**Smokes affected:**
- `libs/image/tests/smoke/graycomatrix_smoke.m` — `[r, c, v] = find(G); … c(k) …`
  fails with `VM: undefined function 'c' (in call to 'c')`.
- `libs/image/tests/smoke/region_smoke.m` — `[c, sz, n2, p] = bwconncomp(A); …`
  fails with `VM: undefined function 'sz' (in call to 'sz')`.

**Symptom:** A variable bound via `[a, b, …] = fn(...)` destructure
is treated by the VM as an unresolved function on a subsequent
`var(k)` index — even though it should be the local variable from
the destructure. Looks like a sibling of bug #1 (the `split.m`
shadowing fix from 2026-05-11) but on the destructure path.

**Note on `region_smoke.m`:** the smoke itself is also semantically
incorrect — `bwconncomp` returns a 1×1 struct, not a 4-output
destructure (that pattern is non-MATLAB). But the parse / VM error
message is masking the underlying type mismatch; should report
"too many output arguments" or similar rather than a phantom
"undefined function" lookup.

**Status:** pre-existing; core/ scope.
**First seen:** present at 2026-05-13 baseline.

---

## 45. `core/parser`: string-literal flag arg `'reverse'` not parsed inside `disp(cummax(A, ''reverse''))` — **P2**

**Smoke:** `libs/stats/tests/smoke/cummax_cummin_smoke.m`
**Symptom:** `Parse error at line 9 col 18: expected ), got 'reverse'`
on the smoke line:
```matlab
disp(cummax(A, ''reverse'')');
```
The doubled single-quotes are an escape attempt inside a string the
smoke author meant to keep literal. Either the smoke needs a
different escape mechanism or the parser should handle this form.

**Impact:** Cosmetic for users (the canonical form
`cummax(A, 'reverse')` parses fine), but blocks this particular
smoke.

**Workaround:** rewrite the smoke without the nested-quote pattern.
**Status:** pre-existing; smoke-author + parser interaction.
**First seen:** present at 2026-05-13 baseline.

---

## 46. `libs/image`: `morph_smoke.m` — `strel(...)` struct used as scalar — **P3**

**Smoke:** `libs/image/tests/smoke/morph_smoke.m`
**Symptom:** Line 8 — `Cannot read element as double from type 'struct'`.
The smoke calls `strel('square', 3)` (returns a struct value) and
then immediately tries to use it where a numeric scalar is expected.

**Root cause:** Smoke-script bug — should use `getnhood(se)` or
access `.Neighborhood` field, not the struct itself.

**Status:** pre-existing; smoke-author bug, not a numkit defect.
**First seen:** present at 2026-05-13 baseline.

---

## Notes

- This file is the bug intake for the parity cycle. When I close one
  (e.g. by fixing in `libs/`), the row stays for history with a
  "Fixed in commit X" line; new bugs go to the bottom.
- Bugs whose root cause sits in `core/` are tracked here for
  visibility but not actioned by this cycle.
- P3 perf gaps surfaced by bulk-bench are documented here too (see #9).
  They are not regressions; they're SIMD-optimization candidates.
