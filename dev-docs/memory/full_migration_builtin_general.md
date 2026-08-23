# Full Migration to Pure C++ Builtin: General Utility & Catalog Functions (`general`)

## Context
As part of the Layer L2 Compute vs Layer L3 Registration architecture, the general utility and catalog introspection functions (`general`) were migrated to pure, engine-free C++ in `src/builtin/` under `namespace numkit::builtin`.

## Architectural Decision & Changes
1. **Public Engine-Free C++ Header**:
   - `include/numkit/builtin/general.hpp` provides public C++ declarations for catalog introspection (`help`, `what`, `builtins`, `categories`) with complete Doxygen documentation.
   - Decoupled from `Engine`, `CallContext`, and VM.

2. **Compute Implementation (`src/builtin/src/general/`)**:
   - `catalog.cpp`: `help`, `what`, `builtins`, `categories` implementations querying `runtime::HelpCatalog`.

3. **Engine Registration (`src/bundle/src/register/builtin/`)**:
   - `general_reg.cpp` registers general builtins (`clc`, `which`, `addpath`, `rmpath`, `path`, `rehash`, `pwd`, `cd`, `mkdir`, `rmdir`, `delete`, `dir`, `ls`, `pathsep`, `formatteddisplaytext`, `format`, `home`, `optimset`, `optimget`, `freqspace`, `help`, `doc`, `what`, `builtins`, `inmem`) under `namespace numkit::bundle::builtin`.
   - Old monolithic `src/builtin/src/general.cpp` was deleted.

## Verification & Results
- Built with `desktop-fast` preset (MSVC).
- Layer validation: `python tools/check_layering.py` passed with 0 violations.
- Test suite: `numkit_gtest.exe --gtest_filter=*General*:*Help*:*What*:*Builtin*` passed 729/729 tests (100% green).
