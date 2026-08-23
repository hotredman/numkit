# Extract ASCII Save/Load into `saveload_ascii.cpp`

## Context & Problem
Previously, `saveload.cpp` mixed two different responsibilities:
1. Top-level CLI flag parsing and dispatcher logic (`save`, `load`, `save_reg`, `load_reg`).
2. The ASCII matrix persistence backend (formatting double matrices and whitespace parsing with comment skipping).

Meanwhile, binary MAT files had their own separate backend in `saveload_mat.cpp`.

## Solution & Architecture
Separated the persistence subsystem in `src/runtime/src/` into three cleanly decoupled components:
- `saveload.cpp`: Pure top-level dispatcher for `save(...)` / `load(...)`, options parsing (`-ascii`, `-mat`, `-v4`, `-v6`, `-v7`), and Engine adapters (`save_reg`, `load_reg`).
- `saveload_ascii.cpp`: Dedicated ASCII text backend implementing `saveAscii(...)` and `loadAscii(...)`.
- `saveload_mat.cpp`: Dedicated zero-dependency binary MAT-file codec implementing `saveMat(...)` and `loadMat(...)`.

## Verification
- `python tools/check_layering.py` verified 0 layering violations.
- All 442 tests in `FileIoTest`, `VfsTest`, `CsvTest`, etc. passed 100%.
