# Help System & HelpCatalog Migration to `bundle`

## Context & Problem
Previously, the help catalog (`HelpCatalog`) and documentation discovery functions were placed in:
1. `src/runtime/include/numkit/runtime/help/help_catalog.hpp` and `src/runtime/src/help/help_catalog.cpp`.
2. `src/builtin/src/general/catalog.cpp` (which depended on `runtime::HelpCatalog`).

This violated clean separation of concerns:
- `runtime` (L2) is the scripting engine / VM execution core and should not hold an aggregate database of all higher-level toolboxes.
- `builtin` (L3) is pure standard math / numerical manipulation and should not depend on `runtime` or an aggregate catalog of L4 toolboxes.
- `bundle` (L5) is the top-level aggregator that composes all toolboxes, `BuiltinLibrary`, `RuntimeLibrary`, and user-facing standard library facilities.

## Decisions & Implementation
1. **Relocated `HelpCatalog` to `numkit::bundle`**:
   - Header: `src/bundle/include/numkit/bundle/help/help_catalog.hpp`.
   - Source: `src/bundle/src/help/help_catalog.cpp`.
2. **Unified Help Library & Discovery API**:
   - Header: `src/bundle/include/numkit/bundle/help.hpp`.
   - Source: `src/bundle/src/help/help.cpp`.
   - Declares `HelpLibrary::install(Engine &engine)` and pure C++ discovery functions `help(...)`, `what(...)`, `builtins(...)`, `categories(...)`.
   - Registers script built-ins: `"help"`, `"doc"`, `"what"`, `"builtins"`, `"categories"`, `"inmem"`.
3. **Cleaned up `StandardLibrary::install`**:
   - `StandardLibrary::install(Engine &engine)` now explicitly installs `bundle::HelpLibrary::install(engine)`.
4. **Cleaned `builtin` and `runtime` layers**:
   - Removed `help_catalog.*` from `src/runtime/`.
   - Removed `catalog.cpp` and `general.hpp` from `src/builtin/`.
   - Removed duplicate `src/bundle/src/register/general/help_reg.cpp`.

## Verification
- `python tools/check_layering.py` verified 0 layering violations across all layers.
- Full compilation and unit tests (`HelpSystemTest`, `GeneralTest`, `BuiltinTest`) passed 100%.
