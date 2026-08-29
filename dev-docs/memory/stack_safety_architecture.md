# Stack Safety Architecture: Eliminating Stack Overflow Crashes on Deep Nesting

## Problem Context
Unbounded nesting in user inputs (e.g. deeply nested function calls such as `sin(sin(...))`, deeply nested blocks `if 1 ... end`, or nested brackets `[[[[...]]]]`) caused process crashes with `0xC00000FD` (`STATUS_STACK_OVERFLOW`) or `SIGSEGV` without diagnostic output.
Investigation revealed that recursive traversals—primarily in the bytecode compiler (`Compiler::compileNodeExpand`), TreeWalker (`TreeWalker::execNodeExpand`, `TreeWalker::execBlock`), and recursive descent parser—consumed native stack frames without upfront limits or early depth checks, failing before reaching compiler register limits or runtime checks.

## Chosen Solution & Architecture
A defense-in-depth architecture was implemented:

1. **Layer 1: Parse-Time Nesting Limit (`NestingGuard` in `Parser`)**
   - Implemented an RAII `NestingGuard` tracking `nestingDepth_` against `MAX_NESTING_DEPTH = 200`.
   - Placed across all recursive parser entry points:
     - `parseExpression()`
     - `parsePrimary()` (handles nested parentheses `(expr)`)
     - `parseStatement()`
     - `parseBlock()`
     - `parseIf()`, `parseFor()`, `parseWhile()`, `parseSwitch()`, `parseTryCatch()`
     - `parseArrayLiteral()`, `parseMatrixLiteral()`, `parseCellLiteral()`
   - Exceeding the limit immediately throws `std::runtime_error` with 1-indexed source line and column numbers.

2. **Layer 2: Physical Stack Watermark Guard (`StackGuard`)**
   - Created `numkit::StackGuard` in `src/core/include/numkit/core/stack_guard.hpp` and `src/core/src/stack_guard.cpp`.
   - Cross-platform physical watermark check guaranteeing at least 64 KB of free stack space for safe C++ exception unwinding:
     - **Windows**: Uses `GetCurrentThreadStackLimits` to verify `currentStack >= lowLimit + 64 KB`.
     - **WASM (Emscripten)**: Uses `emscripten_stack_get_free() >= 64 KB`.
     - **POSIX / Linux / macOS**: Uses `pthread_attr_getstack` / `pthread_get_stackaddr_np` bounds.
   - Hooked in:
     - `Compiler::compileNodeExpand` ("bytecode compilation")
     - `TreeWalker::execNodeExpand` ("tree walker execution")
     - `TreeWalker::execBlock` ("tree walker execution")

3. **Layer 3: Automated Regression Tests**
   - `src/core/tests/parser_test.cpp`: Added `ParserNestingLimitTest` checking expression nesting, parenthesized expressions, control flow blocks, matrix literals, and cell literals.
   - `tests/gtest/integration/error_diagnostics_test.cpp`: Added `DualEngineTest` parametrized tests verifying that deeply nested inputs return clean diagnostics on both TreeWalker (TW) and Bytecode VM backends without crashes, and verifying `StackGuard` API.

## Rationale
- **Nesting limit (200)**: 200 levels exceeds any legitimate real-world mathematical script or algorithm while safely staying below compiler register file capacities (255 registers) and 1 MB default thread stacks.
- **Physical watermark (64 KB)**: Protects against environments with smaller stacks (e.g. WASM web workers, debug builds with unoptimized larger stack frames) and ensures the core soundness invariant: user input either executes cleanly or reports an actionable error diagnostic, never terminating the process unexpectedly.

## Quantitative Verification Results
- `deep_nest.m` ($N=500$ nested `sin(...)`):
  - Result: Clean error `Parse error at line 1 col 797: Expression or block nesting exceeds maximum supported depth (200)`, exit code 1 (no crash).
- `deep_if.m` ($N=5000$ nested `if 1 ... end`):
  - Result: Clean error `Parse error at line 68 col 4: Expression or block nesting exceeds maximum supported depth (200)`, exit code 1 (no crash).
- `deep_mat.m` ($N=300$ nested `[[[[...]]]]`):
  - Result: Clean error `Parse error at line 1 col 50: Expression or block nesting exceeds maximum supported depth (200)`, exit code 1 (no crash).
- Test suite:
  - All 56 targeted error diagnostic and nesting limit tests passed on both TreeWalker and VM backends.
