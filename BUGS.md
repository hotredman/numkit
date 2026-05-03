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

## 10. `libs/signal`: `compat.nextpow2` is scalar-only — **P2**

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

---

## 11. `core/`: `arrayfun(@lambda, vec)` ignores the lambda body — **P1**

**Reproducer:**
```matlab
arrayfun(@(x) x*2, 1:5)     % numkit: [1 2 3 4 5]   ; MATLAB: [2 4 6 8 10]
arrayfun(@(x) x^2, 1:5)     % numkit: [1 2 3 4 5]   ; MATLAB: [1 4 9 16 25]
arrayfun(@(x) sin(x), 1:5)  % numkit: [1 2 3 4 5]   ; MATLAB: sin([1..5])
arrayfun(@(k) nchoosek(30,k), 0:5)
                            % numkit: [0 1 2 3 4 5] ; MATLAB: [1 30 435 ...]
```
**Symptom:** The anonymous-function body is completely bypassed; arrayfun
returns the iterated input value verbatim. Direct calls to the body
work — `nchoosek(30,15)` returns 155117520 correctly. Only the
arrayfun-wrapped form is broken.
**Impact:** Any MATLAB code that uses `arrayfun(@(...)..., ...)` will
silently produce wrong results — not a crash, just the wrong answer.
This is likely the most dangerous parity bug found so far.
**Where:** `core/` — arrayfun dispatch + lambda body application
in TreeWalker / VM. Probably the lambda's captured AST isn't being
re-bound to the `x` arg at each iteration.
**Status:** **pending — core fix required.** Documented; do not attempt
fix from this cycle (libs/ only).
**First seen:** 2026-05-03, parity bulk-bench iteration 7 (probing
`nchoosek` MISMATCH).

---

## 12. `libs/builtin`: `polyder` doesn't strip leading zeros from result — **P3**

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

---

## 13. `libs/builtin`: bit-ops reject integer typed arrays — **P2**

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

---

## 14. `core/`: `(:)` on a logical scalar segfaults — **P0**

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
**Status:** **pending — core fix required.** Not actionable from
this cycle (libs/ only).
**First seen:** 2026-05-03, parity bulk-bench iteration 10 (probing
`strcmp` SAVE-block hang).

---

## 15. `core/`: `fieldnames(struct)` returns alphabetical, not insertion order — **P2**

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
**Status:** **pending — core fix required.**
**First seen:** 2026-05-03, parity bulk-bench iteration 14.

---

## 16. `libs/builtin`: `func2str(@sin)` returns `'@sin'` instead of `'sin'` — **P3**

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

---

## 17. `libs/builtin`: `cellstr(multi_row_char)` flattens column-major instead of row-split — **P2**

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

---

## 18. `libs/builtin`: `cross(A,B)` rejects 3-by-N matrix batch form — **P2**

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

---

## 19. `libs/builtin`: `freqspace(N)` returns wrong-length vector — **P2**

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

---

## 20. `core/`: `shiftdim` doesn't strip trailing singletons — **P2**

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

---

## 21. `libs/builtin`: `meshgrid(xv)` / `interp2(X,Y,...)` 2-D-input forms missing — **P2**

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

---

## 22. `libs/builtin`: `spline(x, v)` (2-arg pp-struct form) unsupported — **P2**

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

---

## 23. `libs/builtin`: `meshgrid(x,y,z)` 3-arg form returns only 2 outputs — **P2**

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

---

## 24. `core/`: `.*` (and other elementwise ops) reject `logical .* double` — **P2**

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
**Status:** **pending — core fix required.**
**First seen:** 2026-05-03, parity bulk-bench iteration 20.

---

## 25. `libs/builtin`: `isStringScalar` (camelCase) not registered — **P3**

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

---

## 26. `libs/builtin`: `num2str(X, fmt)` and `num2str(X, n)` ignore the precision argument — **P2**

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

---

## Notes

- This file is the bug intake for the parity cycle. When I close one
  (e.g. by fixing in `libs/`), the row stays for history with a
  "Fixed in commit X" line; new bugs go to the bottom.
- Bugs whose root cause sits in `core/` are tracked here for
  visibility but not actioned by this cycle.
- P3 perf gaps surfaced by bulk-bench are documented here too (see #9).
  They are not regressions; they're SIMD-optimization candidates.
