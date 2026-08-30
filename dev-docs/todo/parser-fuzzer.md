# todo: Minimal generative parser/interpreter fuzzer

*Kind:* tech-debt · *Status:* open · *Surfaced:* 2026-08-29 (stack-safety design layer 3; deferred repeatedly since)

> Lifecycle: open → done. On completion, record the outcome in
> `dev-docs/memory/` (per the AGENTS.md project-memory protocol) and
> delete this file — the todo list holds open work only.

**Problem.** No fuzzing harness exists for the lexer/parser/compiler/TreeWalker
chain. The stack-safety work proved the class is real (deep nesting killed the
process); arbitrary malformed input may find hangs, lexer loops, assertion
escapes that hand-written tests never will. The engine is a pure
string→result function — trivially fuzzable.

**Why deferred.** Priority went to the differential fieldtest corpus (real
code) and the P1 fix queue.

**Fix.** A gtest with a time budget: mutate the seed corpus (all repo .m files:
smokes, examples, fieldtest work copies) + random token soup; invariant =
parses or throws a diagnostic, never crashes/hangs. Coverage-guided later via
the emscripten/clang build.

**Affected.** New `src/core/tests/parser_fuzz_test.cpp` (wired like
`stack_guard_test.cpp`); seeds from `src/**/tests/smoke/`, `examples/`,
`fieldtest/corpus/work/`.
