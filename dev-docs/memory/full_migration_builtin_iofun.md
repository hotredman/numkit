# Full Migration to Pure C++ Builtin: Input/Output Functions (`iofun`)

## Context
As part of the layer separation refactoring (Layer L2 Compute vs Layer L3 Registration), formatted input/output, stream printing, and string scanning (`iofun`) were migrated to pure, engine-free C++ in `src/builtin/` under `namespace numkit::builtin`.

## Architectural Decision & Changes
1. **Public Engine-Free C++ Header**:
   - `include/numkit/builtin/iofun.hpp` defines the public C++ API with comprehensive Doxygen documentation, parameter descriptions, formulas, exceptions, and `@see` links.
   - Decoupled from `Engine`, `CallContext`, and the VM.

2. **Compute Implementation (`src/builtin/src/iofun/`)**:
   - `formatted_io.cpp`: `sprintf`, `disp`, `fprintf`, `sscanf`, `textscan`.

3. **Engine Registration (`src/bundle/src/register/builtin/`)**:
   - `iofun_reg.cpp` registers all formatted I/O builtins into `Engine` under `namespace numkit::bundle::builtin`.
   - Old monolithic `src/builtin/src/iofun.cpp` was deleted.

## Verification & Results
- Built with `desktop-fast` preset (MSVC).
- Layer validation: `python tools/check_layering.py` passed with 0 violations.
- Test suite: `numkit_gtest.exe --gtest_filter=*Io*:*Scan*:*Print*:*Format*:*Builtin*` passed 1117/1117 tests (100% green).
