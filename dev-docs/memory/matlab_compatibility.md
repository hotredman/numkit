# MATLAB Compatibility Mode (Implicit Imports)

## Problem
In standard `numkit`, functions like `mean`, `std`, and `plot` are housed in toolboxes (e.g. `compat`). Originally, scripts needed an explicit `import compat.*` statement to access these functions, deviating from standard MATLAB behavior where these functions are available in the global namespace by default. 

When users wrote pure scripts without imports, or used `clear all` which cleared the workspace imports, standard library functions became unavailable.

## Decision
We implemented "Sticky Implicit Imports" (`MATLAB Compatibility Mode`) configurable via the IDE or CLI (`--compat`). When enabled, `compat.*` is automatically imported and persists across `clear all` commands.

## Rationale
- **Backwards Compatibility**: Restores drop-in compatibility with existing MATLAB code that expects standard functions to be globally available.
- **Resilience**: Modifying `clearAll()` and the `eval` sync cycle to explicitly restore `implicitImports_` prevents user scripts from breaking themselves when they reset their workspace.

## Implementation Details
- `Engine` class extended with `implicitImports_` vector.
- `syncVMToWorkspace()` and `syncVMToScope()` updated to restore these imports automatically if `clearAllCalled_` is true.
- `workspace.cpp` builtins (`clear`, `clear all`, `clearvars`) invoke `ctx.engine->restoreImplicitImports(env)` immediately upon wiping the workspace.
- `DebugSession::runInDebugScope` captures active imports before evaluating console / watch / condition expressions and restores `preEvalImports` and `implicitImports_` upon completion.
- `DebugSession` captures `engine_.outputFunc()` at `start()` and restores it in `deactivate()` / destructor, preventing dangling lambda pointers and `bad_alloc` crashes in subsequent normal script runs calling `fprintf` / `disp`.
- WASM bindings expose `repl_set_compat_mode(bool)` for the Web IDE.
- CLI executable supports a new `--compat` flag.
- IDE `Settings` UI extended to provide a global toggle for this behavior.

## Impact
All example scripts have been cleaned to remove explicit `import compat.*` lines, confirming the new default behavior correctly simulates MATLAB's global namespace. Debugging workflows (including breakpoints, watches, console evaluations, and step over/into/out) reliably resolve toolbox functions even after `clear` or `clear all`, and transitioning between debug runs and normal runs maintains valid output redirection.
