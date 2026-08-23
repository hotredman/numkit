# Migration of `accumarray` to `builtin::datafun`

## Problem & Context
Previously, `accumarray` and its `AccumReducer` enum were located in `src/runtime/include/numkit/runtime/language/arrays/accum.hpp` and `src/runtime/src/language/arrays/accum.cpp`.
However:
1. `accumarray` is a pure array reduction algorithm that belongs to `datafun` (Data Analysis & Statistics), identical in nature to `groupsummary`, `findgroups`, `cumsum`, `diff`, `histcounts`.
2. The compute algorithm does not depend on `Engine`, VM, or interpreter state.
3. Placing it in `runtime` cluttered the language runtime layer with numerical array reduction code.

## Solution & Decision
1. **Moved pure compute to `builtin::datafun`**:
   - `enum class AccumReducer` and `Value accumarray(...)` declared in `include/numkit/builtin/datafun.hpp`.
   - Pure C++ multi-dimensional accumulation algorithm implemented in `src/builtin/src/datafun/accum.cpp`.
2. **Registration and handle dispatch in `bundle`**:
   - Script-facing adapter `accumarray_reg` implemented in `src/bundle/src/register/builtin/datafun_reg.cpp`, registering `"accumarray"` into the engine.
3. **Cleaned up `runtime`**:
   - Deleted `src/runtime/include/numkit/runtime/language/arrays/accum.hpp` and `src/runtime/src/language/arrays/accum.cpp`.
   - Removed `registerArraysRuntime` from `RuntimeLibrary::install`.
4. **C++ API Coverage**:
   - Added direct C++ API test `src/builtin/tests/accum_public_api_test.cpp`.

## Verification
- `python tools/check_layering.py` verified 0 layering violations.
- Built Release and all unit tests passed.
