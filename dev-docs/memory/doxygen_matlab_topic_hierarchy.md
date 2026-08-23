# Memory: Doxygen MATLAB-Authentic Topic Hierarchy & Theme UX

## Context & Problem
NumKit exposes more than 1,000 public C++ functions and types across 15+ toolboxes and language domains. Previously, Doxygen dumped all functions into large, flat namespace pages (`numkit::builtin`), making logical discovery difficult. Furthermore, the default Doxygen theme featured an oversized header consuming substantial vertical space.

## Solution & Architectural Decisions
1. **Topic Hierarchy (`doxygen/groups.dox`)**:
   - Organized the entire documentation using standard MATLAB categories and toolboxes:
     - **MATLAB Language Fundamentals (`group_matlab`)**: `elmat`, `elfun`, `matfun` (linalg), `datafun`, `specfun`, `polyfun`, `strfun`, `datatypes`, `timefun`, `iofun`.
     - **MATLAB Toolboxes (`group_toolboxes`)**: `signal`, `stats`, `image`, `control`, `optim`, `wavelet`, `comm`, `audio`, `ode`, `graphics`.
   - Annotated all public headers in `src/` with `@ingroup`.
2. **Interactive Landing Page (`doxygen/doxygen_mainpage.dox`)**:
   - Replaced flat text lists with an interactive CSS Grid containing clickable topic cards, feature badges, and a code quick-start snippet.
   - Included standard nominative fair use trademark disclaimer for MATLAB.
3. **Modern Theme UX (`doxygen-awesome-css` + `custom.css`)**:
   - Integrated Doxygen Awesome theme with dark/light mode toggle, copy buttons on code blocks, and interactive table of contents.
   - Reduced header height to ~45px with sleek, vertically centered logo.
   - Added quick links in top bar to GitHub repository (`hotredman/numkit`) and Web IDE (`hotredman.github.io/numkit-demo/`).
4. **Native Deployment Scripts**:
   - `scripts/publish-doxy.bat` and `scripts/publish-doxy.sh` without external Python dependencies.
   - Integrated into `scripts/publish-all.bat` and `scripts/publish-all.sh`.

## Verification & Deployment
- Doxygen generates all `topics.html` and `group__*.html` modules cleanly.
- Static assets published and pushed directly to `git@github.com:hotredman/numkit-doxy.git` on branch `main`.
