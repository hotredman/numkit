# scripts/

Thin wrappers around the project's two toolchains — the **C++ engine** (CMake)
and the **IDE** (npm / Vite / Electron). Naming: `build-*` produces an artifact,
`run-*` launches something. `.sh` = Linux/macOS/git-bash, `.bat` = Windows; where
only one flavour exists, that path is currently used on that platform only
(desktop packaging is Windows-only today).

## Engine (C++ / CMake)

| Script | Does |
|---|---|
| `engine-build.{sh,bat}` | Configure + build the engine. `--fast` → `desktop-fast` preset (Highway SIMD), `--wasm` → `browser` preset (Emscripten; needs `EMSDK`), no arg → `portable`. |
| `test.sh` | Build `desktop-fast` + run the gtest suite via `ctest --preset=desktop-fast`. Args pass through, e.g. `test.sh -R Haart`. |
| `coverage.ps1` | Ninja + clang-cl coverage build → `llvm-cov` report (enters a VS Dev Shell first). |

## IDE (npm / Vite / Electron)

The IDE is **one** Vite app delivered several ways. What decides asset/fetch
resolution is **where `index.html` lands** — i.e. what `base` /
`import.meta.env.BASE_URL` resolves against — *not* "dev vs server" (a dev server
and a hosted build are both HTTP servers). The real axes are source-vs-built and
HTTP-vs-`file://`:

```
Vite DEV server — live source, HMR                        BASE_URL = /
   web-run.{sh,bat}      browser dev server at :3000
   desktop-run.bat       Electron window pointed at that same dev server

vite build → dist/ — static bundle                        BASE_URL = ./ (relative)
   web-build.{sh,bat}    → deploy/ ; host anywhere over HTTP (root OR sub-path)
   desktop-build.bat     → packaged .exe ; Electron loadFile over file://
```

| Script | Does | Transport |
|---|---|---|
| `web-run.{sh,bat}` | Copy WASM → `ide/public/`, run the Vite **dev server** (source + HMR) at `:3000`. | HTTP, root |
| `desktop-run.bat` *(win)* | Launch the Electron shell in **dev mode** — `main.js` spawns the same Vite dev server and loads the window from it. | HTTP, root |
| `web-build.{sh,bat}` | Build the static site (WASM + `vite build`) into `deploy/` (gitignored). Host it anywhere. | HTTP, root or sub-path |
| `publish-code.{sh,bat}` | Push source code, main branch, and tags to GitHub source mirror repository. | Git / GitHub |
| `publish-pages.{sh,bat}` | Synchronize built static site (`deploy/`) into GitHub Pages distribution repository (`--push` to push). | Git / GitHub Pages |
| `publish-doxy.{sh,bat}` | Generate Doxygen API documentation and deploy/push directly to `numkit-doxy` GitHub Pages repo. | Git / GitHub Pages |
| `publish-all.{sh,bat}` | Publish both source code repository and Web IDE GitHub Pages distribution in one command. | Git / GitHub |
| `desktop-build.bat` *(win)* | Full desktop build: WASM + `vite build --base ./` + `electron-builder --win portable` → `.exe`. | file:// |

### Why `base: './'` (relative)

`ide/vite.config.js` sets `base: './'` so the single built `dist/` works in every
target: at a web root, under a sub-path, and over `file://` in the packaged
desktop app. In the Vite **dev** server the base normalises to `/` (dev serves at
root) → `BASE_URL = /`; in any **build** it bakes to `./` (relative). The
Explorer's bundled Examples are fetched as `${BASE_URL}examples/manifest.json`,
so this is exactly what makes them resolve everywhere. `desktop-build.bat` also
passes `--base ./` as an explicit guarantee for the file:// shell.

> GitHub Pages hosting has been retired — there is no `numkit-m` sub-path
> deployment anymore. `web-build` produces a static bundle you host yourself
> (or preview with `npm run preview` from `ide/`).

## Notes

- The examples mirror `ide/public/examples/` is **gitignored and generated** from
  `examples/` by `ide/scripts/generate-manifest.js`, run automatically via the
  npm `predev` / `prebuild` hooks (and by `web-build.*`). An empty
  `public/examples/` just means it hasn't been generated yet — run any IDE script
  and it regenerates; it is **not** a broken state.
- `EMSDK` (WASM build) is read from the environment; the `.bat` wrappers fall
  back to a default path only when it is unset.
