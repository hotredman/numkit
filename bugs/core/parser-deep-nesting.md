# core.parser — deeply nested input crashes the process (stack overflow, no diagnostic)

- **Status:** ✅ FIXED (`e8267458`, 2026-08-29; hardening follow-up — parser watermark + POSIX cache, same day)
- **Severity:** P0 crash (process / WASM module death, no diagnostic)
- **Kind:** bug
- **Found:** 2026-08-29 via a manual deep-nesting probe during a project review (fuzzing discussion)

## Symptom

Input with nesting above the bytecode-compiler's stack budget kills the
process with `0xC00000FD` (`STATUS_STACK_OVERFLOW`) and empty stderr. Three
regimes were measured (desktop-fast Release):

| nesting `sin(sin(...))` | behaviour |
|---|---|
| ≤ ~250 | executes |
| 300–425 | clean error: `Compiler: register exhaustion (>255 registers needed in chunk)` |
| ≥ ~450 | **silent process death** (`0xC00000FD`) |

A second vector — thousands of nested `if 1 ... end` blocks — crashes with no
clean-error window at all. The IDE opens arbitrary files / accepts pasted
input, so the trigger is one Ctrl+V away.

## Repro

```matlab
y = sin(sin( ... sin(1) ... ));   % 500 levels
% numkit (before fix): process exits -1073741571 (0xC00000FD), no output
% expected:           a clean parse diagnostic (nesting limit) or correct execution
%
% MATLAB R2025b reports its own parser-level error past its (unprobed)
% nesting threshold — the contract violated here is "diagnostic, never a
% crash", which numkit honours everywhere else (codegen §10 soundness).
```

## Root cause

Recursive descent in `Compiler::compileNode`/`compileNodeExpand` recurses per
nesting level with no depth/stack guard; the descent dies before the existing
register-exhaustion check (which fires on the way *up*) can run. The TreeWalker
and deep-block parsing have the same unbounded-recursion shape. Full analysis:
[dev-docs/STACK_SAFETY.md](../../dev-docs/STACK_SAFETY.md).

## Fix (e8267458 + follow-up)

Three layers, per STACK_SAFETY.md §5:

1. Parse-time nesting contract — `NestingGuard` in `Parser`, limit 200
   (justified by the `uint8_t` register file, not a magic number); clean
   diagnostic with line/col on both engines.
2. Physical watermark — `StackGuard::check` (64 KB margin; Windows
   `GetCurrentThreadStackLimits`, WASM `emscripten_stack_get_free`, POSIX
   `pthread_attr_getstack` cached per-thread) in `compileNodeExpand`,
   `TreeWalker::execNodeExpand`/`execBlock`, and (follow-up) the parser's
   `NestingGuard` for small-stack hosts.
3. Regression — `ParserNestingLimitTest` (6 cases), `ErrorDiagnosticsTest`
   deep-nesting cases on both engines (`INSTANTIATE_DUAL`), and
   `StackGuardTest` proving the watermark fires before the hardware guard
   page.

Verified after the fix on desktop-fast: all crash vectors (sin×500,
sin×100000, if×5000, `[`×300) → clean diagnostic exit 1, in **both Release and
Debug**; depth ≤ 200 still executes.

## References

- `src/core/include/numkit/core/stack_guard.hpp`, `src/core/src/stack_guard.cpp`
- `src/core/include/numkit/core/parser.hpp` (`NestingGuard`)
- `src/core/tests/parser_test.cpp`, `src/core/tests/stack_guard_test.cpp`,
  `tests/gtest/integration/error_diagnostics_test.cpp`
- `dev-docs/STACK_SAFETY.md` (design + anti-kludge rationale),
  `dev-docs/memory/stack_safety_architecture.md`
