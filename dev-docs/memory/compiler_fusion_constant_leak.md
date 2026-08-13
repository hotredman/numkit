# Compiler Fusion Constant Leak

## Context
During the execution of the `contourf_filled.m` example, the script crashed with:
`Matrix dimensions must agree. a=[60x60], b=[0x0] (in operator '.^')`
The crash happened in the middle of a deeply nested mathematical expression (the classic MATLAB `peaks` function):
```matlab
Z = 3 * (1 - X).^2 .* exp(-X.^2 - (Y + 1).^2) ...
  - 10 * (X/5 - X.^3 - Y.^5) .* exp(-X.^2 - Y.^2) ...
  - exp(-(X + 1).^2 - Y.^2) / 3;
```

## Root Cause Analysis
The issue was initially misdiagnosed as an evaluator AST depth/ScratchArena limit because pre-calculating squared terms into intermediate variables seemed to bypass the crash. 
However, the true root cause was a constant register leak in the JIT bytecode compiler related to **Element-wise Fused Operations (`FUSE_EWISE`)**.

When the compiler encounters a fusible expression (like `(1 - X).^2`, which maps to `sq_affine`), it executes `Compiler::compileFused`. This function performs the following sequence:
1. Evaluates the fusion rule's operands into registers.
2. Emits a `FUSE_EWISE` instruction with a placeholder destination.
3. Compiles the **fallback** normal expression using `compileBinaryOp` (or unary equivalents).
4. Sets the fallback span length in the `FUSE_EWISE` instruction, so the VM can skip the fallback if the fused kernel successfully executes at runtime.

The flaw existed in step 3. The `Compiler` maintains a constant cache map (`constRegCache_`) to avoid duplicate `LOAD_CONST` instructions. When the fallback compilation evaluates a numeric constant (like `2` for the `.^2`), it emits a `LOAD_CONST` instruction inside the fallback block and populates `constRegCache_`.

If the `FUSE_EWISE` kernel **succeeds** at runtime, it skips the fallback block. This means the `LOAD_CONST` instruction inside the fallback is never executed. However, the compiler has already cached that register as holding the constant `2`. 
When subsequent expressions in the same block (e.g. `-X.^2`) require the constant `2`, the compiler fetches the leaked register from `constRegCache_` instead of emitting a new `LOAD_CONST`. Since the original load was skipped, the register remains completely uninitialized (empty value, `[0x0]`). When this empty register is passed to operators like `.^`, it causes dimension mismatch errors (`a=[60x60], b=[0x0]`).

## Solution
Modified `Compiler::compileFused` in `src/core/src/compiler.cpp` to save a copy of `constRegCache_` and `scalarRegs_` before compiling the fallback block, and restore them immediately afterward. This ensures any constants loaded exclusively in the skipped fallback block do not pollute the cache for the remainder of the compilation unit.
