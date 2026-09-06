# core.tests — Debug gtest crashed in DualEngineTest SetUp: gtest suite-name collision (TEST_F + TEST_P both "BuiltinTest")

- **Status:** ✅ FIXED (HASH, 2026-09-06)
- **Kind:** bug
- **Severity:** P2 latent UB (silent in Release — the official full-suite config — crashes every Debug run)
- **Found:** 2026-09-06 during the linprog portion: running the optim suites in the locally built Debug gtest aborted with `SEH exception with code 0xc0000005 thrown in SetUp()`.

## Symptom

In the DEBUG configuration every `TW_VM/BuiltinTest.*` test (the
dual-engine fixture in `src/bundle/tests/builtins_test.cpp`) crashes
with an access violation inside `SetUp()` — i.e. during engine
construction + `eval("import compat.*;")` — on BOTH backends. The same
tests pass in the RELEASE configuration (the config the official
`tests-run` preset builds), which is why the full-suite runs stayed
green while the bug sat underneath.

## Repro

```
cmake --build build/windows/release --target numkit_gtest   # Debug config
build/windows/release/tests/gtest/Debug/numkit_gtest.exe --gtest_filter='TW_VM/BuiltinTest.Zeros/TW'
# => SEH exception with code 0xc0000005 thrown in SetUp()
build/windows/release/tests/gtest/Release/numkit_gtest.exe --gtest_filter='TW_VM/BuiltinTest.Zeros/TW'
# => OK
```

Reproduced with sources as far back as afbedbf82 (2026-09-06 morning)
— predates portions 12/13; exact introducing commit unknown (needs a
Debug-config bisect, the historical full-suite runs were Release).

## Root cause

NOT engine UB (the initial hypothesis — a step-by-step probe of the
fixture SetUp ran clean). The binary contained TWO test suites both
named `BuiltinTest` backed by DIFFERENT fixture classes:
`TEST_F(BuiltinTest, ...)` in `src/builtin/tests/builtin_test.cpp`
(plain fixture) and `TEST_P(BuiltinTest, ...)` in
`src/bundle/tests/builtins_test.cpp` (DualEngineTest). Mixing TEST_F
and TEST_P under one suite name is documented gtest UB — the
registration state corrupts, and the first `TW_VM/BuiltinTest.*` test
crashes with an access violation attributed to SetUp; the corruption
also takes down the next suite that runs after it. Debug builds
manifest it; Release happened to survive, which is why the official
Release-config full-suite runs stayed green while every local Debug
run died.

## Fix

Renamed the one-test plain suite to `BuiltinLayerTest`
(src/builtin/tests/builtin_test.cpp) — no collision remains; a
comment at the class pins the rule. Verified: `TW_VM/BuiltinTest.*`
now passes in Debug; a full Debug-config suite run completes (it had
never completed in this state). cdb turned out to be unusable through
the MSYS shell (no console) — the localization came from an
instrumented same-fixture probe TU + the crash's suite specificity.

Guard: the suites themselves — `TW_VM/BuiltinTest.*` running green in
Debug IS the regression signal (a collision reintroduction crashes the
Debug binary again). Consider an occasional Debug-config CI run so
test-infrastructure UB stops hiding behind the Release default.

## References

- `src/builtin/tests/builtin_test.cpp` (renamed suite)
- `src/bundle/tests/builtins_test.cpp` (the parameterized BuiltinTest)
