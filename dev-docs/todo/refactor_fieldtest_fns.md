# todo: Refactor fieldtest_fns.cpp into domain modules with full MATLAB parity

*Kind:* tech-debt / architecture · *Status:* open · *Surfaced:* 2026-08-30

> Lifecycle: open -> done. On completion, record the outcome in
> `dev-docs/memory/` (per the AGENTS.md project-memory protocol) and
> delete this file — the todo list holds open work only.

## Problem & Rationale
The temporary file `src/builtin/src/datafun/fieldtest_fns.cpp` was introduced as a quick bridge for four functions demanded by GitHub fieldtest scripts (`web`, `genpath`, `vec2ind`, `rands`). However:
1. **Architectural anti-pattern (junk drawer):** It bundles unrelated domains (system shell, filesystem paths, matrix generation, neural net encoding) into a single ad-hoc file under `datafun/`.
2. **MATLAB Parity Divergences:**
   - `rands(S)` returns a row vector ($1 \times S$) instead of MATLAB's column vector ($S \times 1$).
   - `genpath(dir)` only traverses 1 level of subdirectories instead of recursive depth-first search, and does not filter out `@class`, `+package`, `private`, or `.git` directories.
   - `vec2ind(v)` returns `1` for all-zero columns instead of `NaN`.
   - `web` throws an error when called without arguments (`web`), whereas in MATLAB nullary call is valid.

## Implementation Plan

### 1. Relocate `rands` to `src/builtin/src/elmat/`
- Move into `src/builtin/src/elmat/matrix.cpp` alongside `rand`, `randn`, `randi`.
- Fix 1-argument signature: `rands(S)` must return an $S \times 1$ column vector (weight vector convention in MATLAB Neural Network Toolbox).

### 2. Relocate `web` to `src/bundle/src/register/builtin/general_reg.cpp`
- Move alongside system environment builtins (`clc`, `clear`, `diary`, `quit`, `system`).
- Support nullary call: `web` with 0 arguments prints the warning and returns status `0` without throwing a `nargin` error.

### 3. Relocate `genpath` to `src/builtin/src/iofun/`
- Move into `src/builtin/src/iofun/` (or path management module).
- Implement recursive VFS traversal.
- Skip special directories: those starting with `@` (class folders), `+` (package namespaces), `private`, and hidden dot-directories (`.git`).

### 4. Relocate `vec2ind` to `src/builtin/src/datafun/`
- Move into `src/builtin/src/datafun/` (alongside `sub2ind`, `ind2sub`, `accumarray`).
- Fix all-zero column handling: columns without a positive active element must return `NaN`.

### 5. Remove `fieldtest_fns.cpp`
- Remove `src/builtin/src/datafun/fieldtest_fns.cpp` from source trees and `CMakeLists.txt`.
- Update registration declarations in `datafun_reg.cpp` and `general_reg.cpp`.

### 6. Automated Unit Tests
- Add comprehensive gtest test suites covering edge cases for all four functions:
  - `rands`: 1-arg ($S \times 1$), 2-arg ($M \times N$), range verification in $[-1, 1]$.
  - `vec2ind`: standard one-hot matrices, row vectors, all-zero columns (`NaN`), empty inputs `[]`.
  - `genpath`: nested directory trees, excluded `@` and `+` folders.
  - `web`: 0-arg call, 1-arg URL string, return value `0`.

## Acceptance Criteria
- [ ] `src/builtin/src/datafun/fieldtest_fns.cpp` completely removed.
- [ ] `rands(5)` produces a $5 \times 1$ column vector.
- [ ] `vec2ind([0; 0])` returns `NaN`.
- [ ] `genpath` recursively discovers subdirectories while ignoring `@` and `+` folders.
- [ ] All gtest unit tests and smoke tests pass cleanly with zero regressions.
