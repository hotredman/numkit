# Memory: Doxygen MATLAB-Authentic Topic Hierarchy & Theme UX

## Context & Problem
NumKit exposes more than 1,000 public C++ functions and types across 15+ toolboxes and language domains. Previously, Doxygen dumped all functions into large, flat namespace pages (`numkit::builtin`), making logical discovery difficult. Furthermore, the default Doxygen theme featured an oversized header consuming substantial vertical space.

## Solution & Architectural Decisions
1. **Topic Hierarchy (`doxygen/groups.dox`)**:
   - Organized the entire documentation using clean 12 Fundamentals + 10 Toolboxes modules:
     - **Fundamentals (`group_matlab`)**: `elmat`, `elfun`, `ops`, `matfun`, `datafun`, `specfun`, `polyfun`, `strfun`, `datatypes`, `timefun`, `lang`, `iofun`.
     - **Toolboxes (`group_toolboxes`)**: `signal`, `stats`, `image`, `control`, `optim`, `wavelet`, `comm`, `audio`, `ode`, `graphics`.
   - Bound all public functions directly into sub-groups via `@addtogroup` blocks inside header namespaces, ensuring `Fundamentals` and `Toolboxes` display strictly clean topic directories with 0 loose functions.
   - Suppressed raw file tables on topic pages and configured `FULL_PATH_NAMES = NO`.
2. **Interactive Landing Page (`doxygen/doxygen_mainpage.dox`)**:
   - Clean, modern grid of cards under **Fundamentals** and **Toolboxes**.
   - Made header logo mark `[n_k]` and project title clickable links pointing back to `index.html`.
3. **Modern Theme UX (`doxygen-awesome-css` + `custom.css` + `signature-highlighter.js`)**:
   - Injected semantic AST/token highlighting for all C++ prototypes (return types, keywords, parameters, default values, references).
   - Removed right TOC panel for spacious, full-width readability.
4. **Synchronized Help Catalog (`src/bundle/src/help/help_catalog.cpp`)**:
   - Built-in REPL `help` system mirrors the exact 22 topics (Fundamentals + Toolboxes) with full aliases and discoverability.
5. **Unified Script Convention (`<target>-<action>`)**:
   - `code-publish`, `web-run`, `web-build`, `web-publish`, `desktop-run`, `desktop-build`, `doxy-run`, `doxy-publish`, `all-publish`.

## Verification & Deployment
- Automated tests pass in `numkit_gtest` (29 tests in General/Help).
- Full pipeline deployed:
  - Source Code: `https://github.com/hotredman/numkit`
  - Web IDE Demo: `https://hotredman.github.io/numkit-demo/`
  - Doxygen API: `https://hotredman.github.io/numkit-doxy/`
