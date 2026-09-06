# core.engine — gtest DEBUG config crashes with access violation in DualEngineTest SetUp (Release passes)

- **Status:** 🔴 OPEN
- **Kind:** bug
- **Severity:** P2 latent UB (silent in Release — the official full-suite config — crashes every Debug run)
- **Guard:** deferred — the repro ABORTS the Debug process (SEH); nothing assertable until the UB frame is localized. This file is the tracker.
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

Not yet diagnosed. Given the Release/Debug split, almost certainly
undefined behavior that Release tolerates (cf.
bugs/closed/signal/freqs-autogrid-debug-oob, found the same way): an
out-of-bounds read or a stale reference on the engine-construction /
implicit-import path, surfacing only with MSVC Debug heap/iterators.

## Suggested fix

Reproduce under a debugger (cdb wasn't capturable through the shell
pipe — use `cdb -logo <file>` or Visual Studio), get the faulting
frame on the construction/import path, fix the UB at the source.
Consider an occasional Debug-config CI/preset run so this class of
latent bug stops hiding behind the Release default.

## References

- Fixture: `tests/gtest/support/dual_engine_fixture.hpp` (SetUp)
- Sibling precedent: `bugs/closed/signal/freqs-autogrid-debug-oob.md`
