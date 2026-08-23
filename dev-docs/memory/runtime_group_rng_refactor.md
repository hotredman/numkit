# Runtime Grouping & RNG Layering Refactoring

## Problem & Context
Previously, `src/runtime/include/numkit/runtime/math/group/group.hpp` and `src/runtime/src/math/group/group.cpp` resided in `runtime` under a legacy `math/` subdirectory path. Inside `group.hpp`, pure array grouping operations (`findgroups`, `groupcounts`, `groupsummary`, `grouptransform`) were declared in `namespace numkit::builtin`, creating architectural confusion between `runtime` and `builtin`. Furthermore, `group.cpp` combined pure compute, VM pausable callback state machines (`SplitapplyCallbackBuiltin`), and `CallContext` registration adapters (`_reg`). Additionally, `builtin/datafun.hpp` contained dummy non-context-taking random declarations (`rand()`, `randn()`) and a stub `datafun/random.cpp`.

## Chosen Solution & Architecture
1. **Pure Compute in `builtin` (`src/builtin/src/datafun/group.cpp`)**:
   - Declared `FindgroupsResult`, `findgroups`, `GroupcountsResult`, `groupcounts`, `GroupsummaryResult`, `groupsummary`, `grouptransform` in `src/builtin/include/numkit/builtin/datafun.hpp` with complete Doxygen documentation.
   - Implemented pure C++ compute in `src/builtin/src/datafun/group.cpp` (no Engine or CallContext dependencies).
   - Removed empty `src/builtin/src/datafun/random.cpp` stub.

2. **RNG Export in `builtin` (`src/builtin/include/numkit/builtin/datafun.hpp`)**:
   - Exported `RngContext` and the deterministic, thread-safe, seedable RNG generators (`rand`, `randn`, `randND`, `randnND`, `randi`, `randperm`) from `numkit::ops` into `numkit::builtin`.
   - C++ callers use explicit `RngContext &rng` (or `engine.rng()`), preventing hidden global state or mutex bottlenecks.

3. **VM Callback in `runtime` (`src/runtime/src/language/splitapply_callback.cpp`)**:
   - Moved `SplitapplyCallbackBuiltin` and `registerSplitapplyCallbackBuiltin(Engine &engine)` into `numkit::runtime`.
   - Removed the legacy `src/runtime/include/numkit/runtime/math/group/group.hpp` and `src/runtime/src/math/group/group.cpp`.

4. **CallContext Adapters in `bundle` (`src/bundle/src/register/builtin/group_reg.cpp`)**:
   - Placed `findgroups_reg`, `groupcounts_reg`, `groupsummary_reg`, `grouptransform_reg`, `groupfilter_reg`, and `splitapply_reg` in the bundle registration layer.

## Verification
- `python tools/check_layering.py` verified 0 layer violations.
- Built Release and passed all unit test suites.
