# core.compiler — Debug-config suite aborts at CompilerRegisterOverflowTest.OverflowInSingleExpression/TW (Release passes)

- **Status:** 🔴 OPEN
- **Kind:** bug
- **Severity:** P2 latent (Debug-only manifestation; the test is green in the Release config)
- **Found:** 2026-09-06, portion 23b: with the suite-name collision and the ncfrnd poisson issue fixed, the Debug-config full run got further than ever before and now aborts at this test (process dies mid-run, no gtest failure line).

## Symptom

`numkit_gtest.exe` (Debug) aborts during
`TW_VM/CompilerRegisterOverflowTest.OverflowInSingleExpression/TW` — the
test deliberately compiles a huge single expression to trigger
RegisterExhaustionError; in Debug the deeper per-frame stack footprint
(MSVC iterator debug + asserts) appears to blow the stack (or hit an
assert) before the graceful throw. Release runs green.

## Repro

```
cmake --build build/windows/release --target numkit_gtest   # Debug
build/windows/release/tests/gtest/Debug/numkit_gtest.exe --gtest_filter='*OverflowInSingleExpression*'
# → process aborts (no test verdict)
```

## Root cause

Suspected stack exhaustion in the recursive compiler/parser on the
deliberately-oversized expression, Debug stack frames being several×
larger. numkit has a StackGuard (see dev-docs/memory/
stack_safety_architecture.md) — either its threshold doesn't account for
the Debug frame scale on this path, or the abort is an assert inside the
debug heap during AST clone/destruction.

## Suggested fix

Run under a debugger to get the abort frame (SEH catch disabled via
--gtest_catch_exceptions=0 + WER LocalDumps if cdb stays unusable in the
shell). If stack: raise the guard margin for Debug builds (per-config
threshold) or convert the oversize-expression path to iterative
compilation. Consider a Debug-config CI lane so this class stops hiding.

## References

- Test: `CompilerRegisterOverflowTest` (grep src/core/tests)
- Prior Debug-only finds, same pattern: bugs/closed/core/
  debug-build-setup-crash.md, ncfrnd/ncx2/negbin/poisson guards
  (portion 23b commit)
- **Guard:** deferred — the repro ABORTS the Debug process; this file is
  the tracker (the Release green run is the shipping gate).
