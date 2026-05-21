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
exempt — see [§14](#14-internal-helpers-are-exempt).

**How to apply.** Follow these rules for every new function and for any
old function you touch. Do **not** rewrite untouched code wholesale just
to conform — migrate opportunistically.

---

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
| A user-supplied function handle / callback                  | `FnHandle`                |
| An interpreter instance (`Engine *`)                        | **never** — see [§7](#7-no-engine--in-the-public-api) |

The rest of this document is the long form of that table.

---

## 1. Argument order

```cpp
ReturnType name(<required data>,
                <optional data with defaults>,
                <string / enum flags>,
                std::pmr::memory_resource *mr = nullptr);
```

`mr` is **always last**, **always a pointer** (never a reference),
**always defaults to `nullptr`**. `ScratchArena` resolves a null `mr` to
the process-default resource internally — callers never have to.

## 2. Scalar parameters → native C++ types

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

## 3. Array parameters → `const Value &` or `Span<const double>`

Never a raw `(const T *, size_t)` pair in a public signature.

- **`const Value &`** — when the function genuinely cares about the
  element type (real vs complex), about being N-D, or about the input
  *shape* (e.g. row vs column, `m×n` matrix layout). `Value` carries all
  of that; extract inside via typed accessors or `valueToDoubleRow(v,
  arena)`.

- **`Span<const double>`** — when the input is *definitionally* a flat
  real vector and its shape carries no meaning. `Span` bundles
  pointer+length safely and lets a C++ caller pass `{1, 2, 3}`, a
  `std::vector`, or a `std::array` with no ceremony.

```cpp
// shape & dtype matter → Value
Value polystab(const Value &a, mr);

// flat real vector, nothing else → Span
Value combnk(Span<const double> v, int K, mr);
std::pair<Value,Value> pulstran(Span<const double> t,
                                Span<const double> d, FnHandle fn, mr);
```

## 4. Optional `Value` parameters

- Default to `const Value &x = Value::Empty` — MATLAB-style `0×0`
  `DOUBLE`.
- Test inside with `x.isEmpty()` (equivalently `x.numel() == 0`).
- **Forbidden:** `const Value *x = nullptr` in a public signature.

`Value::Empty` is a static-const sentinel (a real empty matrix).
`Value::Unset` is internal-only (default-constructed, "unset variable"
for the parser/VM) and must never be a public default.

## 5. Magic-polymorphism → typed overloads

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
dispatches to the right overload (see [§13](#13-the-engine-adapter-pattern)).

### 5a. Caveat — a genuine *scalar-or-vector* concept is **not**
magic-polymorphism

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

## 6. `FnHandle` for callbacks

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

## 7. No `Engine *` in the public API

A `libs/` function must be callable **without an interpreter
instance**. `Engine *` (and `CallContext &`) must never appear in a
public `libs/` signature. Anything that previously needed the engine to
call back into user code now takes a [`FnHandle`](#6-fnhandle-for-callbacks)
instead.

`Engine *` lives **only** in the interpreter glue — the `*_reg`
adapters — see [§13](#13-the-engine-adapter-pattern).

## 8. Multi-option → options struct

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

## 9. Multi-output — return, never out-pointers

- 2 outputs → `std::pair<Value, Value>`
- 3 outputs → `std::tuple<Value, Value, Value>`
- 4+ outputs, or whenever names aid clarity → a named result struct,
  e.g. `struct BodeResult { Value mag, phase, w; };`
- **Forbidden:** `Value *out0, Value *out1, …` out-arguments.

Structured bindings (`auto [a, b] = f(x)`) map 1:1 onto MATLAB's
`[a, b] = f(x)` and eliminate the null-pointer bug surface.

## 10. Return types — prefer `Value`, native is a soft option

The default return type is `Value` (or `pair` / `tuple` / a named
struct of `Value`s). This keeps the API uniform with MATLAB parity and
avoids surprises.

**Soft recommendation:** a function whose result is *definitionally* a
single scalar **and** is never consumed polymorphically may return a
native `double` / `bool` directly. This is a judgement call, not a
mandate — when in doubt, return `Value`.

## 11. `Value` ergonomic constructors

These exist in `core/value.hpp` and are what make literal call syntax
work — rely on them, don't reinvent:

- `Value(std::initializer_list<double>)` — implicit; drives `{1,2,3}`
  literals.
- `Value(Span<const T>)`, `Value(const std::vector<T> &)`,
  `Value(std::array<T,N>)` — templated over `T ∈ {double, float, int*,
  uint*, std::complex<double>}`.
- `Value::view(...)` — non-owning; caller manages lifetime, copy-on-write
  on mutation.

## 12. Documentation

- Doxygen `///` on every public declaration: `@brief`, `@param`,
  `@return`, `@throws`, and at least one `@code` / `@endcode` example
  for any non-trivial function.
- Inside `.cpp`: comment **why**, never **what**. The code says what.

## 13. The Engine-adapter pattern

The interpreter reaches a `libs/` function through a thin `*_reg`
adapter (registered in `libs/<ns>/src/library.cpp`). The adapter is the
**only** place `Engine *` / `CallContext &` is allowed. It:

1. unpacks runtime `Value` arguments into native types,
2. dispatches magic-polymorphic call sites to the right overload
   ([§5](#5-magic-polymorphism--typed-overloads)),
3. wraps the engine's function-handle machinery into a stack-resident
   `FnHandle` lambda,
4. passes `ctx.engine->resource()` as `mr`.

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

## 14. Internal helpers are exempt

Functions in `namespace detail`, anonymous namespaces, or `.cpp`-local
helpers may use whatever signature is convenient. Only public-facing
declarations in `libs/<ns>/include/**` must follow these rules.

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

// ❌ Value where a plain scalar is meant
Value foo(const Value &x, const Value &count);
// ✅ Value foo(const Value &x, int count);

// ❌ one Value param meaning two unrelated things
Value num2str(const Value &x, const Value &precisionOrFormat);
// ✅ two typed overloads

// ❌ 8+ positional args, no struct
Value foo(int, int, double, int, std::string, double, std::string);
// ✅ options struct
```
