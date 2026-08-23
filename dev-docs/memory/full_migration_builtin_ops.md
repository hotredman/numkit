# Memory: Full Migration of Operator Implementations to `src/builtin/src/ops/`

## Context & Objectives
In accordance with the approved full migration plan (Variant 1: Full Migration), we moved compute implementations out of `src/lang/src/operators/` directly into `src/builtin/src/ops/`.

The objective is to eliminate forwarding layers and make `src/builtin/` the unified compute and standard library layer in `numkit`.

## Changes Made
1. **Source Migration**:
   - Created `src/builtin/src/ops/binary_ops.cpp` and `src/builtin/src/ops/binary_ops_detail.hpp`.
   - Created `src/builtin/src/ops/unary_ops.cpp` and `src/builtin/src/ops/unary_ops_detail.hpp`.
   - Migrated all binary operators (`plus`, `minus`, `times`, `mtimes`, `rdivide`, `ldivide`, `mrdivide`, `mldivide`, `power`, `elementPower`, `mpower`, `pagemtimes`, `eq`, `ne`, `lt`, `gt`, `le`, `ge`, `logical_and`, `logical_or`, `logical_xor`) and unary operators (`uminus`, `uplus`, `logical_not`, `ctranspose`, `transpose`, `any`, `all`) to `namespace numkit::builtin`.
2. **Engine Registration Wiring**:
   - In `src/builtin/src/ops.cpp`, wired `BuiltinLibrary::registerBinaryOps` and `BuiltinLibrary::registerUnaryOps` directly to `numkit::builtin::*`.
3. **Build System & Backward Compatibility**:
   - Added `src/ops/binary_ops.cpp` and `src/ops/unary_ops.cpp` to `NUMKIT_BUILTIN_SOURCES` in `src/builtin/CMakeLists.txt`.
   - Removed them from `src/lang/CMakeLists.txt`.
   - Updated `src/lang/include/numkit/lang/operators/binary_ops.hpp` and `unary_ops.hpp` to forward using `using ::numkit::builtin::*` during migration.

## Verification Results
- `numkit_gtest.exe --gtest_filter=*Pagemtimes*`: 54/54 passed (100%).
- `numkit_gtest.exe --gtest_filter=*BuiltinTest.*:*OpsTest.*`: 854/854 passed (100%).
