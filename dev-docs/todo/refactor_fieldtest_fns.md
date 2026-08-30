# todo: Distribute fieldtest-fns registrations to domain modules + fix MATLAB parity

*Kind:* tech-debt / parity · *Status:* in-progress · *Surfaced:* 2026-08-30

> Lifecycle: open → done. On completion, record the outcome in
> `dev-docs/memory/` and delete this file.

## Problem

`src/bundle/src/register/builtin/fieldtest_fns_reg.cpp` is a junk drawer
bundling four unrelated domains. Plus four MATLAB parity divergences.

## Plan (respecting the layering: registrations need Engine → bundle)

1. **Distribute registrations** to their domain `*_reg.cpp` files:
   - `web` → `general_reg.cpp` (alongside `clc`, `clear`, `system`)
   - `genpath` → `paths_reg.cpp` (path management)
   - `vec2ind` → `datafun_reg.cpp` (alongside `sub2ind`/`ind2sub`)
   - `rands` → `elmat_reg.cpp` (alongside `rand`/`randn`)
2. **Delete `fieldtest_fns_reg.cpp`**
3. **Fix all four parity issues:**
   - `rands(S)` → S×1 column vector (MATLAB NN toolbox convention)
   - `genpath(dir)` → recursive DFS; skip `@class`/`+pkg`/`private`/dot-dirs
   - `vec2ind([0;0])` → NaN for all-zero columns
   - `web` nullary → valid, returns 0, no nargin throw
4. **Tests** for each function's edge cases

## Acceptance criteria
- [ ] `fieldtest_fns_reg.cpp` deleted
- [ ] `rands(5)` → 5×1 column
- [ ] `vec2ind([0;0])` → NaN
- [ ] `genpath` recursive + filtered
- [ ] `web` nullary valid
- [ ] Full suite + corpus green
