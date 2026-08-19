# M-File Cold Resolution Call-Frame Return Mode

## Problem & Context
When invoking an m-file function for the first time in a session where the function declares output return values (e.g. `function x = myfun()`) but does not assign them in its body, and the caller invokes the function as a statement with zero output capture (e.g. `myfun();`, `nargout=0`), the first run threw:
```
Error: Too many output arguments.
```
However, on subsequent (warm) runs, the function executed without error.

## Root Cause
In `src/core/src/vm.cpp`:
1. On the **warm/cached** path (`found = findCompiledFunc(funcName)`), the opcode `CALL` passed `pushCallFrame(*targetChunk, &R[argBase], na, I.a, nargout_val)` with default parameters `isMulti = false` and `nout = 0`.
2. On the **cold** path (m-file fallback lookup via `lookupUserFunction`), the opcode `CALL` and `CALL_FLATTEN` erroneously invoked:
   ```cpp
   pushCallFrame(*found, &R[argBase], na, 0, 1, true, I.a, 1);
   ```
   This forced `isMultiReturn = true` and `nout = 1`.
3. When `myfun` exited via `RET`, `popCallFrameMulti` checked `if (frame.isMultiReturn)`:
   `produced = retVal.isUnset() ? 0 : 1;`
   Since `x` was never assigned, `produced` was 0. Because `frame.nout` was forced to 1, `produced < frame.nout` evaluated to true, throwing `"Too many output arguments."`.
4. On subsequent runs, `myfun` was found in `findCompiledFunc`, hitting the warm path where `isMultiReturn = false`, so no error was thrown.

## Solution
1. In `src/core/src/vm.cpp`, updated `CALL` and `CALL_FLATTEN` m-file resolution paths to call `pushCallFrame(*found, args, nargs, I.a, nargout_val)` matching the warm path.
2. Cached the resolved chunk into `resolvedFuncs[funcIdx] = found` so subsequent opcode dispatches within the same chunk hit the direct cache.
3. Added unit test `UnassignedOutputWithZeroNargoutFirstRun` in `src/core/tests/mfile_resolver_test.cpp` testing both TreeWalker and VM backends on first (cold) and subsequent (warm) runs.
