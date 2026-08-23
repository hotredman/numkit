# Full Migration to Pure C++ Builtin: Language & Flow Builtins (`lang`)

## Context
As the final step of the standard library refactoring (Layer L2 Compute vs Layer L3 Registration architecture), language keywords, variable name validation, and environment variables (`lang`) were migrated to pure, engine-free C++ in `src/builtin/` under `namespace numkit::builtin`.

## Architectural Decision & Changes
1. **Public Engine-Free C++ Header**:
   - `include/numkit/builtin/lang.hpp` provides public C++ declarations for environment variables and keyword validation (`setenv`, `getenv`, `iskeyword`, `keywords`, `isvarname`) with full Doxygen documentation.
   - Decoupled from `Engine`, `CallContext`, and VM.

2. **Compute Implementation (`src/builtin/src/lang/`)**:
   - `environment.cpp`: `setenv`, `getenv`
   - `keywords.cpp`: `iskeyword`, `keywords`, `isvarname`

3. **Engine Registration (`src/bundle/src/register/builtin/`)**:
   - `lang_reg.cpp` registers language builtins (`setenv`, `getenv`, `coder`, `coder_run`, `system`, `runNative`, `error`, `warning`, `lastwarn`, `MException`, `rethrow`, `throw`, `assert`, `feval`, `__nk_fwd_call__`, `iskeyword`, `isvarname`) under `namespace numkit::bundle::builtin`.
   - Old monolithic `src/builtin/src/lang.cpp` was deleted.

## Verification & Results
- Built with `desktop-fast` preset (MSVC).
- Layer validation: `python tools/check_layering.py` passed with 0 violations.
- Test suite: `numkit_gtest.exe --gtest_filter=*Lang*:*Keyword*:*Env*:*Varname*` passed 82/82 tests (100% green).
