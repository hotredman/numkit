# scripts/

Thin wrappers around the project's two toolchains — the **C++ engine** (CMake)
and the **IDE** (npm / Vite / Electron). Naming: `build-*` produces an artifact,
`run-*` launches something. `.sh` = Linux/macOS/git-bash, `.bat` = Windows; where
only one flavour exists, that path is currently used on that platform only
(desktop packaging is Windows-only today).

## Engine (C++ / CMake)

| Script | Does |
|---|---|
| `build-engine.{sh,bat}` | Configure + build the engine. `--fast` → `desktop-fast` preset (Highway SIMD), `--wasm` → `browser` preset (Emscripten; needs `EMSDK`), no arg → `portable`. |
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
   run-web.{sh,bat}      browser dev server at :3000
   run-desktop.bat       Electron window pointed at that same dev server

vite build → dist/ — static bundle                        BASE_URL = ./ (relative)
   build-web.{sh,bat}    → deploy/ ; host anywhere over HTTP (root OR sub-path)
   build-desktop.bat     → packaged .exe ; Electron loadFile over file://
```

| Script | Does | Transport |
|---|---|---|
| `run-web.{sh,bat}` | Copy WASM → `ide/public/`, run the Vite **dev server** (source + HMR) at `:3000`. | HTTP, root |
| `run-desktop.bat` *(win)* | Launch the Electron shell in **dev mode** — `main.js` spawns the same Vite dev server and loads the window from it. | HTTP, root |
| `build-web.{sh,bat}` | Build the static site (WASM + `vite build`) into `deploy/` (gitignored). Host it anywhere. *(Was `deploy.*` — it no longer deploys, it just builds a bundle.)* | HTTP, root or sub-path |
| `build-desktop.bat` *(win)* | Full desktop build: WASM + `vite build --base ./` + `electron-builder --win portable` → `.exe`. | file:// |

### Why `base: './'` (relative)

`ide/vite.config.js` sets `base: './'` so the single built `dist/` works in every
target: at a web root, under a sub-path, and over `file://` in the packaged
desktop app. In the Vite **dev** server the base normalises to `/` (dev serves at
root) → `BASE_URL = /`; in any **build** it bakes to `./` (relative). The
Explorer's bundled Examples are fetched as `${BASE_URL}examples/manifest.json`,
so this is exactly what makes them resolve everywhere. `build-desktop.bat` also
passes `--base ./` as an explicit guarantee for the file:// shell.

> GitHub Pages hosting has been retired — there is no `numkit-m` sub-path
> deployment anymore. `build-web` produces a static bundle you host yourself
> (or preview with `npm run preview` from `ide/`).

## Notes

- The examples mirror `ide/public/examples/` is **gitignored and generated** from
  `examples/` by `ide/scripts/generate-manifest.js`, run automatically via the
  npm `predev` / `prebuild` hooks (and by `build-web.*`). An empty
  `public/examples/` just means it hasn't been generated yet — run any IDE script
  and it regenerates; it is **not** a broken state.
- `EMSDK` (WASM build) is read from the environment; the `.bat` wrappers fall
  back to a default path only when it is unset.
