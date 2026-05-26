# numkit `libs/*` — Public API Conventions

These rules govern every **public** function declared in a
`libs/<ns>/include/numkit/<ns>/**/*.hpp` header.

**Why this document exists.** `libs/` is a real C++ numerical library,
not just a backend for the interpreter. An external caller must be able
to write

```cpp
auto y = numkit::signal::firpm(30, {0, 0.4, 0.5, 1}, {1, 1, 0, 0});
auto r = numkit::optim::fzero(myCallback, /*x0=*/1.0);
```

without dragging in an `Engine`, without raw pointer pairs, and without
guessing which of two unrelated meanings a `const Value &` argument
carries. That ergonomic bar drives every rule below.

**Scope.** Rules apply to *public* signatures only. Internal helpers
(`namespace detail`, anonymous namespaces, `.cpp`-local functions) are
exempt — see [§21](#21-internal-helpers-are-exempt).

**API stability.** The `libs/` C++ API is **unstable until numkit 1.0** —
public signatures may change without a deprecation cycle. After 1.0,
changes to a public signature follow semantic versioning with a
documented deprecation period.

**How to apply.** Follow these rules for every new function and for any
old function you touch. Do **not** rewrite untouched code wholesale just
to conform — migrate opportunistically.

The document has two parts:

- **§0 — Scope:** what we build (functions only, no user-OOP classes).
- **§1-§6 — MATLAB parity & provenance:** which functions we build, how
  we research and test them, error behavior, and the clean-room rules.
- **§7-§21 — C++ signature conventions:** how to shape the public C++
  signature.

A worked example to copy is linked at the end.

---

# MATLAB parity & provenance

## 0. Scope — functions only, no MATLAB-style classes

numkit `libs/` ships **MATLAB functions only**. Out of scope:

  * MATLAB-style OOP classes (`classdef`, methods, properties, events,
    inheritance). Examples: `table`, `timetable`, `categorical`,
    `containers.Map`, `dictionary`, `datetime`, `duration`,
    `function_handle` class methods, `cvpartition` object,
    `qrandstream` object, `digraph`/`graph` objects, all
    `*` System Objects, `gpuArray`, etc.
  * Anything that requires `obj = ClassName(...)` plus
    `obj.method(...)` syntax on user-defined types.

When a MATLAB function NAME exists but is class-bound (e.g.
`readtable` returns a `table`), it stays out of scope until the
relevant class is in scope — mark `❌` with the note "needs <class>"
in PROGRESS.md. Plain functions that *consume* numeric/cell/struct
arrays remain fully in scope.

Rationale: numkit's value type is a tagged variant; adding user-OOP
would multiply the type-dispatch surface, fragment the calling
convention, and conflict with §13 (no `Engine` in the public API).
Helper structs returned by parity functions (e.g. `RobustfitResult`,
`GlmfitResult`) are **plain aggregates**, not classes — see §15.

## 1. Replicate the MATLAB API in full

If a function exists in MATLAB, the numkit implementation must reproduce
its **entire observable API** — every calling signature, every optional
and name-value argument, every output, all documented defaults, and the
documented edge-case behavior — matching **MATLAB R2025b**. When MATLAB
and Octave disagree, code to MATLAB. Error behavior is part of the
observable API too — see [§6](#6-error-parity).

"Replicate the API" governs **observable behavior at the interpreter
boundary** — what `.m` code may call and what it gets back. The C++
public signature is a *separate layer*: it stays idiomatic per §7-§21. A
MATLAB function with a polymorphic argument is split into typed C++
overloads ([§11](#11-magic-polymorphism--typed-overloads)); the `*_reg`
adapter ([§20](#20-the-engine-adapter-pattern)) recombines them so the
interpreter-level function still accepts everything MATLAB accepts.

Partial coverage shipped knowingly is allowed **only** when the gap is
explicitly recorded in `PROGRESS.md` — never shipped silently as if
complete.

## 2. Research from MATLAB documentation, not guesswork

Before writing a line of code, probe the reference: run `help <fn>` and
open `doc <fn>` in MATLAB R2025b. Enumerate every signature, option, and
edge case from the documentation first; *then* probe actual behavior in
MATLAB; *then* implement. Working from memory or assumption produces
wrong-formula iterations — the docs come first.

## 3. Test every documented branch against MATLAB

A gtest unit test **and** a smoke `.m` are both mandatory (see
[CLAUDE.md](../CLAUDE.md) for the full four-artefact rule). Together they
must cover **one case per documented branch** — where a "documented
branch" is a distinct entry in `help` / `doc`: a separate calling
signature, a distinct value of an option, or a distinct edge-case
behavior. Cover every branch; do **not** attempt the cartesian product
of all argument combinations.

Expected values are taken from **MATLAB R2025b**, never hand-derived:
run the function in MATLAB and compare numkit's output case by case.
Hand-written expectations have shipped real bugs that only the
cross-engine parity check caught — trust the reference engine, not your
own arithmetic.

## 4. Never use MATLAB source code

Every implementation is **clean-room**. Reading, copying, transcribing,
translating, or paraphrasing MATLAB's own `.m` files or built-in source
— in any form, including decompiled, disassembled, or leaked copies —
is strictly forbidden. Implement only from public documentation,
observed black-box behavior, and independent references (§5).

## 5. Cite original references in the implementation

The implementation file states its sources in a header comment: the
original scientific papers, textbooks, and published standards the
algorithm is derived from (e.g. Oppenheim & Schafer for a DSP filter,
the relevant IEEE / RFC standard for a codec). This records the
clean-room provenance and is **not optional** — a parity function with
no cited source is incomplete.

## 6. Error parity

Error behavior is part of the API — §1 parity covers it. A public
function reports a misuse or domain error the same way MATLAB does:

- **Throw `numkit::Error`** — never return an error code, never set an
  out-flag. Errors propagate as exceptions.
- **Use a MATLAB-style identifier** `m:<fn>:<Reason>`, e.g.
  `m:polyscale:BadScale`. The `<Reason>` tag mirrors MATLAB's own error
  identifier where one exists.
- **Match the message text** to MATLAB's wording where practical, so a
  user porting `.m` code sees familiar diagnostics.
- Every error condition is **enumerated in §1's parity scope**,
  documented with a Doxygen `@throws` ([§19](#19-documentation)), and
  **covered by an `EXPECT_THROW` case** in the gtest ([§3](#3-test-every-documented-branch-against-matlab)).

Input that MATLAB accepts must not throw; input that MATLAB rejects must
throw with the corresponding identifier. "Silently returns a wrong
answer" is the worst outcome — worse than a throw.

---

# C++ signature conventions

## The parameter decision tree

Pick the parameter type by what the argument *means*, not by what is
convenient to extract:

| The argument is…                                            | Use                       |
|-------------------------------------------------------------|---------------------------|
| A scalar with one fixed meaning                             | native (`double`, `int`, `bool`, `std::string`, `std::complex<double>`) |
| An array where dtype / complex / N-D / shape matters        | `const Value &`           |
| Definitionally a flat real vector, shape carries no meaning | `Span<const double>`      |
| One concept that is *scalar **or** vector* (MATLAB broadcast)| `const Value &`           |
| One name historically covering *two unrelated* meanings     | split into typed overloads|
| A user-supplied function handle / callback                  | `FnHandle` ([§12](#12-fnhandle-for-callbacks)) |
| An interpreter instance (`Engine *`)                        | **never** — see [§13](#13-no-engine--in-the-public-api) |

The rest of this part is the long form of that table.

## 7. Argument order

```cpp
ReturnType name(<required data>,
                <optional data with defaults>,
                <string / enum flags>,
                std::pmr::memory_resource *mr = nullptr);
```

`mr` is **always last**, **always a pointer** (never a reference),
**always defaults to `nullptr`**. `ScratchArena` resolves a null `mr` to
the process-default resource internally — callers never have to.

## 8. Scalar parameters → native C++ types

A parameter with a single fixed scalar meaning takes a native type, not
`const Value &`:

```cpp
// ✅
Value zpk(const Value &z, const Value &p, double k, double Ts, mr);
Value combnk(int N, int K, mr);
Value integral(FnHandle fn, double a, double b, double absTol, mr);

// ❌ — `k`, `N`, `a`, `b` are plain scalars; Value adds nothing but
//      an unwrap call and a "what shape is this?" question.
Value zpk(const Value &z, const Value &p, const Value &k, ...);
```

Native scalar types: `double`, `int`, `bool`, `std::string` (for string
flags), `std::complex<double>`.

## 9. Array parameters → `const Value &` or `Span<const double>`

Never a raw `(const T *, size_t)` pair in a public signature.

- **`const Value &`** — when the function genuinely cares about the
  element type (real vs complex), about being N-D, or about the input
  *shape* (e.g. row vs column, `m×n` matrix layout). `Value` carries all
  of that; extract inside via typed accessors or `valueToDoubleRow(v,
  arena)`.

- **`Span<const double>`** — when the input is *definitionally* a flat
  real vector and its shape carries no meaning. `Span` bundles
  pointer+length safely and lets a C++ caller pass `{1, 2, 3}`, a
  `std::vector`, or a `std::array` with no ceremony. `Span` is
  non-owning — never store a `Span` parameter past the call.

```cpp
// shape & dtype matter → Value
Value polystab(const Value &a, mr);

// flat real vector, nothing else → Span
Value combnk(Span<const double> v, int K, mr);
std::pair<Value,Value> pulstran(Span<const double> t,
                                Span<const double> d, FnHandle fn, mr);
```

## 10. Optional `Value` parameters

- Default to `const Value &x = Value::Empty` — MATLAB-style `0×0`
  `DOUBLE`.
- Test inside with `x.isEmpty()` (equivalently `x.numel() == 0`).
- **Forbidden:** `const Value *x = nullptr` in a public signature.

`Value::Empty` is a static-const sentinel (a real empty matrix).
`Value::Unset` is internal-only (default-constructed, "unset variable"
for the parser/VM) and must never be a public default.

## 11. Magic-polymorphism → typed overloads

When one `const Value &` parameter historically encoded **two unrelated
meanings**, split it into overloads with distinct, self-documenting
types:

```cpp
// ❌ before — `spec` is a count OR a printf format; `v` is a range
//             bound OR an explicit element list.
Value num2str(const Value &x, const Value &spec, mr);
Value combnk(const Value &v, int K, mr);
Value fzero(FnHandle fn, const Value &x0OrInterval, mr);

// ✅ after — each overload says exactly what it takes.
Value num2str(const Value &x, int precision, mr);
Value num2str(const Value &x, const std::string &fmt, mr);

Value combnk(int N, int K, mr);                 // K-combos of 1..N
Value combnk(Span<const double> v, int K, mr);  // K-combos of v

Value fzero(FnHandle fn, double x0, mr);            // initial guess
Value fzero(FnHandle fn, double a, double b, mr);   // bracket [a,b]
```

The interpreter `*_reg` adapter inspects the runtime `Value` and
dispatches to the right overload (see
[§20](#20-the-engine-adapter-pattern)).

### 11a. Caveat — a genuine *scalar-or-vector* concept is **not** magic-polymorphism

If a parameter legitimately accepts "scalar **or** vector" as **one
coherent concept** — the MATLAB broadcast idiom — keep it a single
`const Value &` (or `Span`). Splitting it would be wrong.

```cpp
// `scale` is one concept: a scalar applies uniformly, a length-N
// vector applies per-coefficient. Same operation, variable arity.
Value polyscale(const Value &p, const Value &scale, mr);

// `k` is one concept: a scalar window, or a [nb na] two-element
// asymmetric window.
Value movvar(const Value &x, const Value &k, ...);
```

The test: **unrelated meanings → overloads; one concept, variable
arity → one parameter.**

## 12. `FnHandle` for callbacks

Any function that calls back into user-supplied code (`fzero`,
`integral`, `fminsearch`, `cellfun`, `structfun`, `bootstrp`,
`arrayfun`, …) takes a `numkit::FnHandle`, defined in
[`core/fn_handle.hpp`](../core/include/numkit/core/fn_handle.hpp):

```cpp
using FnHandle = function_ref<void(Span<const Value>           args,
                                   Span<Value>                 outs,
                                   std::pmr::memory_resource * mr)>;
```

- **`args` first, `outs` last, `mr` last** — same ordering discipline as
  everywhere else.
- **Outputs are caller-allocated.** The callback fills
  `outs[0 .. outs.size())`; `outs.size()` plays the role of MATLAB's
  `nargout`. The callback never *returns* a `Span` — a returned span
  would have no owning storage.
- **`mr` threads through.** The callback must construct its output
  `Value`s with the supplied `mr` so PMR allocation chains end-to-end.
- **`function_ref` is non-owning.** A `FnHandle` is a 2-pointer view
  valid only for the duration of the library call. Never store one past
  the call.

C++ caller:

```cpp
auto root = numkit::optim::fzero(
    [](Span<const Value> args, Span<Value> outs,
       std::pmr::memory_resource *mr) {
        double x = args[0].toScalar();
        outs[0]  = Value::scalar(x * x - 2.0, mr);
    },
    /*x0=*/1.0);
```

## 13. No `Engine` in the public API

A `libs/` function must be callable **without an interpreter
instance**. `Engine &` / `Engine *` / `CallContext &` must not appear in
a public `libs/` signature. Anything that needs to call back into user
code takes a [`FnHandle`](#12-fnhandle-for-callbacks) instead; `Engine`
lives in the interpreter glue — the `*_reg` adapters
([§20](#20-the-engine-adapter-pattern)).

**Two narrow exceptions**, each because the engine *owns* a resource the
function fundamentally needs:

1. **`install(Engine &)`** in every `library.hpp` — the registration
   hook, not a numerical function.
2. **I/O through the engine's text sink / fid table.** Console and file
   output must route through the engine so an embedder's output
   redirection and capture work. This covers all of `libs/io`, plus
   `disp` / `fprintf` / `fscanf` / `textscan` / `warning`. Such a
   function carries a `// lint: engine-io` marker and, where one is
   meaningful, pairs with a pure engine-free variant (e.g. `dispFormat`
   beside `disp`, `sscanf` beside `fscanf`).

The [API lint](#the-api-lint) enforces this — a new `Engine` parameter
outside those two exceptions fails the build.

## 14. Multi-option → options struct

Three or more optional parameters → group them into a struct with
in-class defaults:

```cpp
struct AdaptHistEqOptions {
    int    numTilesR = 8;
    int    numTilesC = 8;
    double clipLimit = 0.01;
};
Value adapthisteq(const Value &I, const AdaptHistEqOptions &opts = {},
                  std::pmr::memory_resource *mr = nullptr);
```

## 15. Multi-output — return, never out-pointers

- 2 outputs → `std::pair<Value, Value>`
- 3 outputs → `std::tuple<Value, Value, Value>`
- 4+ outputs, or whenever names aid clarity → a named result struct,
  e.g. `struct BodeResult { Value mag, phase, w; };`
- **Forbidden:** `Value *out0, Value *out1, …` out-arguments.

Structured bindings (`auto [a, b] = f(x)`) map 1:1 onto MATLAB's
`[a, b] = f(x)` and eliminate the null-pointer bug surface. How the
adapter decides how many of these to keep is [§16](#16-nargout-dependent-behavior).

## 16. `nargout`-dependent behavior

Many MATLAB functions change what they compute based on how many outputs
the caller requests (`size`, `sort`, `max`, `eig`, …). The C++ library
does **not** branch on `nargout`:

- The library function **always computes and returns the full
  documented output set** — as a `pair` / `tuple` / named struct (§15).
- The `*_reg` adapter ([§20](#20-the-engine-adapter-pattern)) is the
  **only** place `nargout` is consulted: it discards the outputs the
  caller did not request.
- A raw `int nargout` parameter must **never** appear in a public
  library signature.

**Escape hatch.** When one output is genuinely expensive *and* commonly
unwanted — the textbook case is eigenvectors in `eig`, computed only for
`[V, D] = eig(A)` — do not compute it unconditionally. Split into two
functions / overloads, each named for what it returns, so the cost is
opt-in. This is the §11 principle (explicit over hidden polymorphism)
applied to outputs.

## 17. Return types

- A **parity function** (one that exists in MATLAB) **always returns
  `Value`** — or a `pair` / `tuple` / named struct of `Value`s. The
  interpreter needs a `Value` regardless, and MATLAB callers chain
  results polymorphically; a native return type would break both.
- A **non-parity, pure-C++ utility** may return a native `double` /
  `bool` / `int` when its result is definitionally that scalar.

No judgement call: if it exists in MATLAB, it returns `Value`.

## 18. `Value` ergonomic constructors

These exist in `core/value.hpp` and are what make literal call syntax
work — rely on them, don't reinvent:

- `Value(std::initializer_list<double>)` — implicit; drives `{1,2,3}`
  literals.
- `Value(Span<const T>)`, `Value(const std::vector<T> &)`,
  `Value(std::array<T,N>)` — templated over `T ∈ {double, float, int*,
  uint*, std::complex<double>}`.
- `Value::view(...)` — non-owning; caller manages lifetime, copy-on-write
  on mutation.

## 19. Documentation

- Doxygen `///` on every public declaration: `@brief`, `@param`,
  `@return`, `@throws`, and at least one `@code` / `@endcode` example
  for any non-trivial function.
- Inside `.cpp`: comment **why**, never **what**. The code says what.
- The implementation file header cites its references — see
  [§5](#5-cite-original-references-in-the-implementation).

### 19a. Trademark hygiene in published docs

Public Doxygen comments are generated into the published HTML API
reference — the product's public face. "MATLAB" is a registered
trademark of The MathWorks, Inc., and numkit is a commercial product;
treat trademark use in *published* docs with care.

- **Do not narrate behavior through MATLAB.** A `@brief` / `@details`
  describes what the function *does*, intrinsically — not "MATLAB's
  `fzero` does X" or "as MATLAB does". The reader wants the function's
  own contract, not a running comparison.
- **One centralized compatibility statement.** The factual claim
  "compatible with MATLAB R2025b semantics", with the trademark
  attribution, lives **once** on the Doxygen main page
  (`docs/doxygen_mainpage.dox`) — it is not repeated in every `@brief`.
- **Nominative mentions are allowed but sparing.** Where naming the
  reference is genuinely the clearest thing to say — a `@see`, a
  one-line compatibility note on a specific quirk — a factual mention
  is fine. The rule bans *narration*, not *facts*.
- **Internal text is unrestricted.** `.cpp` comments, `PROGRESS.md`,
  parity specs, `CLAUDE.md`, `CONTRIBUTING.md`, and this document name
  MATLAB freely — it is the reference engine and must be named there.
- Clean-room files cite *original papers and standards* as their source
  ([§5](#5-cite-original-references-in-the-implementation)), never
  "MATLAB".

**Deliberately not linted.** A mechanical "no MATLAB token" check would
only push contributors to delete the word to pass the build — a blind
scrub that destroys provenance information. This is a PR-review item
(see the checklist), applied with judgement and migrated
opportunistically, never as a mass find-and-replace.

## 20. The Engine-adapter pattern

The interpreter reaches a `libs/` function through a thin `*_reg`
adapter (registered in `libs/<ns>/src/library.cpp`). The adapter is the
**only** place `Engine *` / `CallContext &` is allowed. It:

1. unpacks runtime `Value` arguments into native types,
2. dispatches magic-polymorphic call sites to the right overload
   ([§11](#11-magic-polymorphism--typed-overloads)),
3. wraps the engine's function-handle machinery into a stack-resident
   `FnHandle` lambda,
4. consults `nargout` to discard unrequested outputs
   ([§16](#16-nargout-dependent-behavior)),
5. passes `ctx.engine->resource()` as `mr`.

```cpp
void fzero_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx) {
    auto handle = args[0];
    auto cb = [&ctx, &handle](Span<const Value> a, Span<Value> o,
                              std::pmr::memory_resource * /*mr*/) {
        auto r = ctx.engine->callFunctionHandleMulti(handle, a, o.size());
        for (size_t i = 0; i < o.size() && i < r.size(); ++i)
            o[i] = std::move(r[i]);
    };
    auto *mr = ctx.engine->resource();
    // dispatch on arity: scalar guess vs [a b] bracket
    outs[0] = (args[1].numel() == 2)
                  ? optim::fzero(cb, args[1].elemAsDouble(0),
                                 args[1].elemAsDouble(1), mr)
                  : optim::fzero(cb, args[1].toScalar(), mr);
}
```

## 21. Internal helpers are exempt

Functions in `namespace detail`, anonymous namespaces, or `.cpp`-local
helpers may use whatever signature is convenient. Only public-facing
declarations in `libs/<ns>/include/**` must follow these rules.

---

## A worked example

`adapthisteq` (the §7-§21 pilot, landed in commit `93b205cc`) is the
reference implementation to copy the *shape* of:

- [`libs/image/include/numkit/image/contrast/contrast.hpp`](../libs/image/include/numkit/image/contrast/contrast.hpp)
  — `const Value &` input, an `AdaptHistEqOptions` struct (§14), `mr`
  last, full Doxygen.
- [`libs/image/src/contrast/contrast.cpp`](../libs/image/src/contrast/contrast.cpp)
  — clean-room implementation with a cited-reference header (§5) and the
  `*_reg` adapter (§20).
- [`libs/image/tests/adapthisteq_test.cpp`](../libs/image/tests/adapthisteq_test.cpp)
  — engine-level gtest, one `TEST_F` per documented branch (§3).
- [`libs/image/tests/adapthisteq_cpp_api_test.cpp`](../libs/image/tests/adapthisteq_cpp_api_test.cpp)
  — exercises the C++ API directly, without the interpreter.
- [`libs/image/tests/smoke/adapthisteq_smoke.m`](../libs/image/tests/smoke/adapthisteq_smoke.m)
  — hand-runnable smoke.

For the `FnHandle` callback pattern (§12), see the worked example in the
header of [`core/fn_handle.hpp`](../core/include/numkit/core/fn_handle.hpp).

---

## Anti-pattern cheat-sheet

```cpp
// ❌ mr first / mr by reference
Value foo(std::pmr::memory_resource *mr, const Value &x);
Value foo(const Value &x, std::pmr::memory_resource &mr);

// ❌ Engine in a library signature
Value foo(const Value &x, Engine *engine);

// ❌ raw pointer-pair array
Value foo(int N, const double *F, size_t Fn);
// ✅ Span<const double> F   (or const Value & if shape/dtype matters)

// ❌ pointer optional
Value foo(const Value &x, const Value *fs = nullptr);
// ✅ const Value &fs = Value::Empty

// ❌ out-pointers for multi-output
void foo(const Value &x, Value *yu, Value *yl);
// ✅ std::pair<Value, Value> foo(const Value &x);

// ❌ int nargout in a library signature
Value foo(const Value &x, int nargout);
// ✅ always return the full set; the *_reg adapter trims

// ❌ Value where a plain scalar is meant
Value foo(const Value &x, const Value &count);
// ✅ Value foo(const Value &x, int count);

// ❌ one Value param meaning two unrelated things
Value num2str(const Value &x, const Value &precisionOrFormat);
// ✅ two typed overloads

// ❌ error reported via return code / out-flag
int foo(const Value &x, Value *out);
// ✅ throw numkit::Error with a m:<fn>:<Reason> identifier

// ❌ 8+ positional args, no struct
Value foo(int, int, double, int, std::string, double, std::string);
// ✅ options struct
```

---

## The API lint

`tools/lint/check_api.py` enforces the *mechanically checkable* subset
of these rules over every `libs/<ns>/include/**/*.hpp` header:

- **§13** — no `Engine` / `CallContext` by-ref/by-ptr in a public
  signature (honouring the two exceptions above).
- **§7** — `memory_resource` passed by pointer, never by reference.
- **§10** — no `const Value *` in a public signature.

Run it directly or via the build:

```sh
python tools/lint/check_api.py     # exit 0 = clean, 1 = violations
cmake --build build/<preset> --target lint
```

The human-judgement rules — §3 test coverage, §5 reference citations —
cannot be linted; the PR checklist below covers them.

## PR checklist

Copy into the pull-request description and tick every box for each
public function added or changed:

```
- [ ] §1  Every MATLAB signature / option / output / default replicated
          (or the gap is recorded in PROGRESS.md).
- [ ] §2  Researched from `help` + `doc` in MATLAB R2025b.
- [ ] §3  gtest + smoke cover one case per documented branch;
          expected values taken from MATLAB R2025b.
- [ ] §4  Clean-room — no MATLAB source consulted.
- [ ] §5  Implementation header cites original references.
- [ ] §6  Errors throw numkit::Error with a m:<fn>:<Reason> identifier;
          each @throws has an EXPECT_THROW case.
- [ ] §7-§17  Signature follows the parameter decision tree
          (mr last, native scalars, FnHandle, no Engine*, overload
          split, options struct, full-output return).
- [ ] §19 Doxygen complete on every public declaration.
- [ ] §19a Public Doxygen describes the function intrinsically —
          no new behavior-narration through MATLAB.
- [ ] Build green; full gtest + smoke suites pass (pre-existing
          failures only).
```
