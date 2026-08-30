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

## Locked decision: hierarchical registration

Registration stays hierarchical () — sub-namespaces
mirror the source structure and MATLAB documentation taxonomy. The bare-name
resolver is the user-facing access mechanism (step 3 finds  →
 via shortNameIndex). The primary namespaceOrder_
path handles future root-level registrations; the fallback handles the
hierarchical case; the memoization cache makes both O(1) after first lookup.

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
- **Deterministic Priority Chain**: derived from the actual registration
  order in `installStandardLibrary()` (already deterministic — libraries
  install in a fixed sequence), NOT hardcoded as a separate constant.
  The resolver iterates namespaces in the order they were registered;
  the first namespace containing the name wins. This means:
  - No priority-list constant to maintain
  - Adding a new toolbox automatically participates
  - The order is visible in `installStandardLibrary()` source code
- **O(1) Resolver Memoization Cache**: Resolved bare names are cached in
  `bareNameCache_` (`std::unordered_map<std::string, const FunctionBinding*>`).
  **Cache invalidation** — the cache entry for name `N` is cleared when:
  - a user defines a function named `N` (`userFuncs_[N] = ...`)
  - a user deletes a function named `N`
  - a new namespace is registered (full cache clear — rare, startup-only)
  This ensures 0 ns overhead on hot loop iterations while preserving
  correctness when the binding changes.
- Name collisions across namespaces: first namespace in the registration
  order that contains the name wins; `which fn` tells the user where it
  came from. Users can disambiguate with `import ns.*` or qualified calls.

**Estimated scope**: ~40-60 lines in engine.cpp (resolver + cache +
invalidation hooks) + `Engine::registeredNamespaces()` enumeration method.

### Phases 2-4: See companion todo

The compat.* elimination, --compat removal, and script cleanup are
detailed in `eliminate_compat_namespace_and_legacy.md` — that todo
owns Phases 2-4. This file owns Phase 1 (the resolver) only.

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

## What this eliminates (combined with the companion todo)

See `eliminate_compat_namespace_and_legacy.md` for the full removal
inventory. The resolver makes all of these possible; the companion
todo executes the cleanup.

## Risk assessment

| Risk | Mitigation |
|---|---|
| Name collision across namespaces | first-registered wins; `which` reports source; user can `import ns.*` to disambiguate |
| Resolver performance (searching all ns) | memoization cache (O(1) after first lookup); ~14 namespaces only on cold miss |
| Cache staleness after user defines a function | invalidation hooks on userFuncs_ changes and new namespace registrations |
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
