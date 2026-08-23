# Flatten Runtime Directory Structure

## Context & Decision
The `src/runtime` module previously contained historical, deeply nested directory hierarchies (`src/language/cells/`, `src/language/structures/`, `src/language/commands/`, `src/programming/errors/`, `src/language/datatypes/`, `src/language/handles/`, `src/language/strings/`), where most directories contained only a single file.

These nested directories were artifacts from an older monolithic layout before `builtin` and `toolboxes` separation.

Since `runtime` (L2) is a compact, cohesive scripting and execution environment, both its public headers (`src/runtime/include/numkit/runtime/`) and source files (`src/runtime/src/`) were flattened into single-level directories.

## Final Structure

### Headers: `src/runtime/include/numkit/runtime/`
- `cell.hpp`
- `containers.hpp`
- `diagnostics.hpp`
- `env.hpp`
- `io.hpp`
- `runtime.hpp`
- `saveload.hpp`
- `struct.hpp`

### Sources: `src/runtime/src/`
- `_handlefn_helpers.hpp`
- `cell.cpp`
- `containers.cpp`
- `diagnostics.cpp`
- `env.cpp`
- `eval.cpp`
- `function_handles.cpp`
- `print_io.cpp`
- `reflection.cpp`
- `runtime.cpp`
- `saveload.cpp`
- `saveload_mat.cpp`
- `scan_io.cpp`
- `splitapply_callback.cpp`
- `struct.cpp`
- `workspace.cpp`

## Verification
- `python tools/check_layering.py` verified 0 layering violations.
- Full test suite passed (882/882 tests).
