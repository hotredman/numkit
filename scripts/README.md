# scripts/

Thin wrappers around the project's toolchains — the **C++ engine** (CMake)
and the **IDE** (npm / Vite / Electron).

Scripts are organized by operating system:
- **`scripts/windows/`** — Windows NT command scripts (`.cmd`) and PowerShell tools (`coverage.ps1`).
- **`scripts/linux/`** — POSIX bash scripts (`.sh`) for Linux and macOS (compatible with WSL).

Naming convention:
- `*-build` produces an artifact (compiles engine, builds web/desktop distribution).
- `*-run` launches an application, dev server, or test suite.
- `*-publish` handles publishing or synchronization with remotes and registries.

## Engine & Tests (C++ / CMake)

| Windows (`scripts/windows/`) | Linux/macOS (`scripts/linux/`) | Description |
|---|---|---|
| `engine-build.cmd` | `engine-build.sh` | Build the engine. Flags: `--wasm` (WASM via Emscripten), `--debug`, `--portable` (no SIMD), default = release with Highway SIMD. |
| `tests-run.cmd` | `tests-run.sh` | Incremental build + run gtest suite (`numkit_gtest`). Forwards arguments to gtest (e.g. `--gtest_filter=*Ordqz*`). Use `--build-only` to skip running. |
| `rebuild-all.cmd` | `rebuild-all.sh` | From-scratch rebuild: wipes build dirs, rebuilds release engine, runs test suite, refreshes npm dist (`--wasm` adds web bundle). |
| `coverage.ps1` | — | Ninja + clang-cl coverage build → `llvm-cov` report (enters VS Dev Shell first). |

## IDE & Tooling (npm / Vite / Electron)

| Windows (`scripts/windows/`) | Linux/macOS (`scripts/linux/`) | Description |
|---|---|---|
| `web-run.cmd` | `web-run.sh` | Copy WASM → `ide/public/`, run Vite **dev server** at `:3000`. |
| `web-build.cmd` | `web-build.sh` | Build static web distribution (WASM + `vite build`) into `deploy/web/`. |
| `desktop-run.cmd` | `desktop-run.sh` | Launch Electron desktop shell in **dev mode** with live reload. |
| `desktop-build.cmd` | `desktop-build.sh` | Build packaged Electron desktop application (`.exe` on Windows, `AppImage` / dir on Linux). |
| `doxy-run.cmd` | `doxy-run.sh` | Generate Doxygen docs (if needed) and serve locally on `http://localhost:8080/`. |
| `bugs-run.cmd` | `bugs-run.sh` | Generate static bugs catalog site and serve locally at `http://localhost:8081/`. |

## Publishing & Deployment

| Windows (`scripts/windows/`) | Linux/macOS (`scripts/linux/`) | Description |
|---|---|---|
| `code-publish.cmd` | `code-publish.sh` | Push source code, main branch, and tags to GitHub public mirror. |
| `web-publish.cmd` | `web-publish.sh` | Deploy built web static distribution to GitHub Pages (`--push` to push). |
| `doxy-publish.cmd` | `doxy-publish.sh` | Generate and deploy Doxygen API docs to `numkit-doxy` Pages repo (`--push` to push). |
| `bugs-publish.cmd` | `bugs-publish.sh` | Generate and deploy bugs catalog site to `numkit-bugs` Pages repo (`--push` to push). |
| `npm-publish.cmd` | `npm-publish.sh` | Refresh WASM package and publish `packages/numkit` to npm (`--dry-run` to preview). |
| `all-publish.cmd` | `all-publish.sh` | One-shot publish: code, web, doxy, and bugs sites (`--push` to push). |

### Why `base: './'` (relative)

`ide/vite.config.js` sets `base: './'` so the single built `dist/` works in every
target: at a web root, under a sub-path, and over `file://` in the packaged
desktop app. In the Vite **dev** server the base normalises to `/` (dev serves at
root) → `BASE_URL = /`; in any **build** it bakes to `./` (relative). The
Explorer's bundled Examples are fetched as `${BASE_URL}examples/manifest.json`,
so this is exactly what makes them resolve everywhere. Desktop build scripts also
pass `--base ./` as an explicit guarantee for the file:// shell.

## Notes

- The examples mirror `ide/public/examples/` is **gitignored and generated** from
  `examples/` by `ide/scripts/generate-manifest.js`, run automatically via the
  npm `predev` / `prebuild` hooks (and by `web-build.*`). An empty
  `public/examples/` just means it hasn't been generated yet — run any IDE script
  and it regenerates; it is **not** a broken state.
- `EMSDK` (WASM build) is read from the environment; the `.bat` wrappers fall
  back to a default path only when it is unset.
