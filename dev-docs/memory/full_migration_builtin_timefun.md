# Full Migration to Pure C++ Builtin: Time & Date Functions (`timefun`)

## Context
As part of the layer separation refactoring (Layer L2 Compute vs Layer L3 Registration), time and date operations, high-resolution stopwatch timers, calendar rendering, date vector conversions, and Julian date conversions (`timefun`) were migrated to pure, engine-free C++ in `src/builtin/` under `namespace numkit::builtin`.

## Architectural Decision & Changes
1. **Public Engine-Free C++ Header**:
   - `include/numkit/builtin/timefun.hpp` defines the public C++ API with comprehensive Doxygen documentation, parameter descriptions, formulas, exceptions, and `@see` links.
   - Decoupled from `Engine`, `CallContext`, and the VM.

2. **Compute Implementation (`src/builtin/src/timefun/`)**:
   - `clock.cpp`: `now`, `date`, `clock`, `cputime`, `pause`.
   - `dates.cpp`: `civilToSerial`, `serialToCivil`, `etime`, `weeknum`, `addtodate`, `datenum`, `weekday`, `juliandate`, `mjuliandate`, `eomday`, `calendar`, `datestr`, `datevec`, `yyyymmdd`.

3. **Engine Registration (`src/bundle/src/register/builtin/`)**:
   - `timefun_reg.cpp` registers all time/date builtins into `Engine` under `namespace numkit::bundle::builtin`, handling engine `tic`/`toc` timers and multi-output unpacking for `datevec` and `weekday`.
   - Old monolithic `src/builtin/src/timefun.cpp` was deleted.

## Verification & Results
- Built with `desktop-fast` preset (MSVC).
- Layer validation: `python tools/check_layering.py` passed with 0 violations.
- Test suite: `numkit_gtest.exe --gtest_filter=*Timefun*:*Builtin*` passed 694/694 tests (100% green).
