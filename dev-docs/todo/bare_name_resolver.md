# todo: Bare-name resolver — eliminate compat.* and make all toolbox functions globally available (MATLAB semantics)

*Kind:* architecture / breaking · *Status:* open · *Surfaced:* 2026-08-30 (architectural discussion with user)

> Lifecycle: open → done. On completion, record the outcome in
> `dev-docs/memory/` and delete this file.

## Problem

numkit has **two registration mechanisms** — a historical artifact of the
layering refactor, not a design decision:

| Mechanism | How | Visibility | Count |
|---|---|---|---|
| Toolbox `reg()` | `reg("signal", "fft", &fn)` → `signal.fft` + `compat.fft` | requires `import compat.*` | ~655 |
| Builtin direct | `engine.registerFunction("fft", &fn)` | globally visible | ~100 |

**Consequence**: ~655 toolbox functions (fft, mean, eig, butter, …) are
invisible to any script that doesn't `import compat.*`. Every real-world
MATLAB script from GitHub fails on "Undefined function" for functions that
ARE implemented. The `compat.*` namespace is a **duplicate flat registration**
masquerading as a namespace — it exists because the engine's name resolver
cannot find bare names across registered namespaces.

**Existing patches on top** (all to be removed):
- 13 bare-name `engine.registerFunction("", "fft", ...)` calls in signal_library.cpp
- 1 bare-name genpath hack in io_library.cpp (added 2026-08-30)
- `--compat` CLI flag (implicit compat import)
- `import compat.*` boilerplate in synthetic corpus scripts

## Design principle

> **Every function registers in its namespace — this is the law.**
> The engine's name resolver finds bare names by searching all registered
> namespaces. This IS MATLAB compatibility — not a mode, not a flag.

MATLAB has toolboxes (Signal Processing, Statistics, …) but you never
`import` them. `fft` just works. The engine must do the same.

## Implementation plan

### Phase 1: Resolver — bare-name fallback

**File**: `src/core/src/engine.cpp` (the name-resolution path)

Add a fallback step to `Engine::resolveFunction(name)`:

```
existing resolution:
  1. exact match in current scope (user-defined, local)
  2. explicit imports (`import signal.*` → search signal namespace)

new fallback (step 3):
  3. bare-name search: iterate all registered function namespaces,
     find the first that contains `name`
     → deterministic: namespace registration order
     → `which(name)` reports the winning namespace
```

Key invariants:
- User-defined functions **always win** over namespace functions (step 1)
- Explicit imports **win** over bare-name fallback (step 2 > step 3)
- Fully qualified names **always resolve directly**: `signal.fft(x)` and `stats.mean(x)` continue to work as explicit disambiguation without going through bare-name search
- **Deterministic Priority Chain**: Namespaces are registered in a fixed priority order:
  `builtin` → `linalg` → `signal` → `stats` → `io` → `image` → `audio` → `control` → `wavelet` → `optim` → `geometry`
- **$O(1)$ Resolver Memoization Cache**: Resolved bare names are cached in `bareNameCache_` (`std::unordered_map<std::string, const FunctionBinding*>`) and invalidated only on new dynamic registrations or user function definitions. This ensures 0 ns overhead on hot loop iterations.
- Name collisions across namespaces: first registered in the priority chain wins; `which fn`
  tells the user where it came from.

**Estimated scope**: ~40-60 lines in engine.cpp (resolver + cache) + `Engine::registeredNamespaces()`.

### Phase 2: Remove compat.* infrastructure

**2a. Remove compat registration from the `reg()` helper**

Every `*_library.cpp` has a lambda like:
```cpp
auto reg = [&](const char *sub, const char *name, ExternalFunc fn) {
    engine.registerFunction(std::string("signal.") + sub, name, fn);
    engine.registerFunction("compat", name, fn);      // ← DELETE THIS LINE
};
```

Files affected (5 libraries, ~5 one-line deletions):
- `src/bundle/src/register/signal/signal_library.cpp`
- `src/bundle/src/register/stats/stats_library.cpp`
- `src/bundle/src/register/linalg/linalg_library.cpp`
- `src/bundle/src/register/io/io_library.cpp`
- `src/bundle/src/register/control/` (if it uses the same pattern)

**2b. Remove existing bare-name overrides**

- 13 `engine.registerFunction("", "fft", ...)` calls in signal_library.cpp
- 1 `engine.registerFunction("genpath", ...)` in io_library.cpp (my hack)

**2c. Remove the compat namespace from the engine**

If the engine has a special-cased "compat" namespace in its import
resolution, remove it. The bare-name resolver makes it redundant.

### Phase 3: Remove --compat flag

**File**: `apps/numkit/main.cpp`

The `--compat` flag does `engine.addImplicitImport({{"compat"}, true, ""})`.
After Phase 2, the compat namespace doesn't exist — the flag becomes a no-op.

Options:
- **Remove the flag entirely** (breaking change for anyone using it)
- **Keep as no-op** with a deprecation warning

Recommendation: remove it. The `compat.*` namespace is gone; keeping a
flag that references it is confusing. Anyone who was using `--compat`
was doing so to get bare-name access — which now works by default.

### Phase 4: Clean up scripts and docs

**4a. Synthetic corpus** (`examples/`)

Remove `import compat.*;` from all example scripts (it was boilerplate).
Scripts should work identically without it.

**4b. Smoke tests** (`src/**/tests/smoke/`)

Same — remove any `import compat.*` lines. The 710-smoke sweep should
pass identically (they already run without compat since the builtin
consolidation).

**4c. WASM bindings** (`wasm/src/repl_bindings.cpp`)

If `repl_init()` or `setenv('NUMKIT_FS', ...)` reference compat, clean up.

**4d. Documentation**

- AGENTS.md: remove `--compat` references; document the bare-name resolver
- dev-docs/handbook/: update any compat references
- packages/numkit/README.md: remove `--compat` from CLI usage

### Phase 5: Verification

**5a. Targeted tests**

- Resolver test: bare name finds namespace function (`fft` → `signal.fft`)
- Precedence test: user-defined `fft` overrides `signal.fft`
- Import test: `import signal.*; filter(x)` → uses signal.filter even if
  control.filter registered first
- Collision test: two namespaces with same name → first registered wins
- `which fft` → reports `signal.fft`

**5b. Full test suite** (on user request)

All 13,153 tests must pass. The dual-engine tests (TreeWalker + VM) exercise
the resolver through both backends.

**5c. Smoke sweep** (710 files)

All smokes must pass — they already run without compat.

**5d. Synthetic corpus** (182 scripts)

All examples must pass — `import compat.*` removed, nothing else changes.

**5e. Fieldtest corpus** (38 scripts)

The critical validation: real GitHub code that never had `import compat.*`.
Expected: many `absent-fn` verdicts become `pass` or progress further —
functions like `mean`, `std`, `butter` are now findable.

**5f. MCP server**

The MCP server uses the same WASM engine; verify `numkit_eval` can call
toolbox functions without any compat mechanism.

## What this does NOT change

- Function **implementations** — zero changes to how functions compute
- Function **signatures** — zero changes to public APIs
- Layering architecture — registration still lives in bundle, compute in toolboxes
- Namespace **existence** — `signal.fft` still a valid qualified name
- Explicit `import signal.*` — still works, still useful for clarity/conflicts

## What this eliminates

| Removed | Why |
|---|---|
| `compat.*` namespace (entire) | duplicate flat registration — resolver makes it redundant |
| `--compat` CLI flag | was enabling compat.*; compat.* is gone |
| `import compat.*` in scripts | namespace doesn't exist; becomes no-op (import of nonexistent ns) |
| 13 bare-name overrides in signal | were patches for the same problem |
| 1 genpath bare-name hack in io | my patch for the same problem |
| `addImplicitImport({{"compat"}, ...})` | the mechanism --compat used |

## Risk assessment

| Risk | Mitigation |
|---|---|
| Name collision across namespaces | first-registered wins; `which` reports source; user can `import ns.*` to disambiguate |
| Resolver performance (searching all ns) | ~14 namespaces, each a hash map lookup — nanoseconds; memoize if needed |
| Breaking change for `--compat` users | flag was transitional; bare-name now works by default (better outcome) |
| Hidden dependency on compat.* | full test suite + 710 smokes + 182 corpus + fieldtest will catch any |

## Acceptance criteria

- [ ] `fft([1 2 3 4])` works in REPL with zero imports, zero flags
- [ ] `signal.fft([1 2 3 4])` continues to work as an explicit qualified call
- [ ] `mean(x)` works (stats namespace, same)
- [ ] `stats.mean(x)` continues to work as an explicit qualified call
- [ ] User-defined `fft` overrides `signal.fft`
- [ ] `import signal.*; filter(x)` uses signal.filter explicitly
- [ ] `which fft` reports `signal.fft`
- [ ] `bareNameCache_` memoizes lookups: subsequent calls in loops execute in $O(1)$ without namespace scanning
- [ ] `compat.*` namespace does not exist in the engine
- [ ] `--compat` flag removed
- [ ] 13,153 gtest cases pass
- [ ] 710 smokes pass
- [ ] 182 corpus scripts pass (without `import compat.*`)
- [ ] Fieldtest: `absent-fn` count decreases (toolbox functions now findable)
- [ ] MCP `numkit_eval("fft([1 2])")` works without compat
