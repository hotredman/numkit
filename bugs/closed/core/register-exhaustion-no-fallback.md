# core.compiler — register exhaustion (>255) surfaces as a user error on real code instead of the TreeWalker fallback

- **Status:** ✅ FIXED (HASH, 2026-09-07)
- **Severity:** P2 (works in MATLAB, refused in numkit)
- **Kind:** bug
- **Found:** 2026-08-30 via fieldtest (real-world `sa_tsp.m` — simulated
  annealing TSP); minimal shape distilled 2026-08-31

## Symptom

```
Error: Compiler: register exhaustion (>255 registers needed in chunk)
```

MATLAB R2025b runs the same code to completion. The documented design
(CORE_ARCHITECTURE / dual-engine) says VM compile failure — including
register exhaustion — falls back to the TreeWalker reference engine; here
the error surfaced to the user instead.

## Repro (self-contained)

```matlab
clear;
y = [1 1 1 1 ... 1];   % a row literal with 253 elements
disp(numel(y));
% numkit: Error: Compiler: register exhaustion (>255 registers needed in chunk)
% MATLAB R2025b: 253
```

Exact ceiling (binary-searched 2026-08-31): **252 elements compile, 253
fail** (253 + chunk overhead > 255). The same error reproduces for a
253-operand `1+1+…` chain and a 300-argument call — every shape that keeps
>~252 values live in one chunk.

Generate the literal: `python -c "print('y = [' + ' '.join(['1']*253) + '']; disp(numel(y));')" > r.m && numkit r.m`

## Root cause (hypotheses to check)

1. The TreeWalker fallback exists only for FUNCTION bodies, not top-level
   scripts (script chunk compile throws straight through);
2. or the fallback triggers but the TreeWalker itself also fails, and the
   original compile error is what gets reported;
3. or the expression legitimately needs that many live registers and the
   compiler could reuse them (register-lifetime bug) — a real compiler fix
   (a matrix literal does NOT need 253 simultaneously-live registers: each
   element can be folded into the accumulator as it is emitted).

Hypothesis 3 is the most likely real defect: the literal builder appears to
allocate one register per element instead of accumulating.

## Suggested fix

Whatever the branch: a real-world script that MATLAB executes must never
die on an internal register ceiling. For matrix literals specifically,
accumulate element-by-element (constant live set). The fallback contract
(register exhaustion → TreeWalker) must hold at every throw site in
`src/core/src/compiler.cpp` (currently 3: preImportGlobals,
pre-allocation of assigned vars, chunk alloc).

## References

- **Guard:** `DISABLED_RegisterExhaustionMatrixLiteralFallsBack` in
  `src/core/tests/vm_test.cpp` (asserts the 253-element literal evaluates;
  remove the prefix when fixed).
- Related: stack_safety.md (the same >255 `uint8_t` register-file ceiling).


## Fix (portion 27, 2026-09-07) — layered, per the pre-proposal evaluation

**A (root: literals in O(1) registers).** compileMatrixLiteral
restructured: rows/columns ≤ 16 keep the block+HORZCAT/VERTCAT fast
path; larger ones compile via the ACCUMULATOR (LOAD_EMPTY seed +
HORZCAT_APPEND/VERTCAT_APPEND per element) with the compileBlock
mark/reset register-release pattern between elements (constRegCache_
and scalarRegs_ cleared at each reset — the cache-leak the first
proposal missed). New VERTCAT_APPEND opcode (VM) + Value::appendScalarCol
({n,1} in-place growth, geometric capacity). Scalar appends amortised
O(1); matrix-valued elements generic two-element concat (O(N²) for
exotic all-matrix literals — documented). The row compiler is a shared
lambda so the vertical accumulator and the block path produce
identical semantics.

**B (contract: top-level TreeWalker fallback).** runOneChunk catches
RegisterExhaustionError and dispatches the AST to the TreeWalker
against workspaceEnv — the documented dual-engine contract, previously
missing at the top level. Under an attached debugObserver it
RE-THROWS (silently switching to the TW would disable breakpoints
without any signal).

**C (function bodies).** The accumulator compiles big literals inside
functions too — the registerFunctionAs exhaustion throw is no longer
reachable from literal shapes; it remains as the loud guard for
genuinely exotic in-function register pressure (documented).

Verified: all five shapes from the analysis give their MATLAB values
(same-literal 300, distinct 300, column 300, +chain 300 → 300,
function-body 300). Guards: RegisterExhaustionAllShapesCompile (5
shapes + value checks) + LiteralThresholdSemanticsIdentical (16/17
threshold boundary, 2D, 20x20 both-accumulators) + the original
MatrixLiteralFallsBack (flipped live) + OverflowInSingleExpression
updated to the new contract (evaluates 500500 via fallback, both
backends). Full Release exit 0, 13235/13237.
