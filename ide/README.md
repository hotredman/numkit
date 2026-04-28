# numkit mIDE — React + Vite Edition

Web-based REPL for the numkit (MATLAB-like) interpreter, built with React + Vite.

## Features

- **Terminal** — interactive command line with history, autocomplete, multiline
- **Plotting** — `plot()`, `bar()`, `scatter()`, `hist()` via D3.js
- **Script Editor** — multi-tab editor with line numbers, run button
- **File I/O** — open/save `.m` files from disk
- **GitHub Browser** — browse any GitHub repo, preview and run `.m` files
- **Variable Inspector** — live workspace with types, sizes, previews
- **Examples** — categorised runnable code snippets
- **Cheat Sheet** — quick reference panel
- **Built-in Help** — `help sin`, `help plot`, etc.

## Quick Start (Development)

```bash
# 1. Install dependencies
npm install

# 2. Start dev server (runs in fallback/demo mode without WASM)
npm run dev
```

Open http://localhost:3000 — the REPL works immediately in demo mode
with a built-in JavaScript interpreter.

## Connecting Your WASM Engine

To use your real C++ numkit interpreter:

### Step 1 — Build WASM with Emscripten

```bash
# From your project root. Requires EMSDK env var set.
cmake --preset=browser
cmake --build --preset=browser
```

This produces two files under `build-browser/wasm/dist/`:
- `numkit_mide.js`   (Emscripten glue code)
- `numkit_mide.wasm` (compiled binary)

### Step 2 — Copy into Vite project

```bash
cp build-browser/wasm/dist/numkit_mide.js   ide/public/
cp build-browser/wasm/dist/numkit_mide.wasm ide/public/
```

Alternatively just run `./build.sh --wasm` (Linux/macOS) or `build.bat --wasm` (Windows), then `./dev.sh` / `dev.bat` — the dev-run scripts copy the artifacts automatically.

### Step 3 — Restart dev server

```bash
cd repl-vite
npm run dev
```

The app auto-detects `window.createNumkitMideModule` and switches from
fallback mode to the real WASM engine.

## Production Build

```bash
npm run build     # outputs to dist/
npm run preview   # preview production build locally
```

Deploy the `dist/` folder to any static host (Netlify, Vercel,
GitHub Pages, your own server).

## Project Structure

```
numkit-mide-vite/
├── index.html              # Entry point (loads WASM script + React)
├── package.json
├── vite.config.js
├── public/
│   ├── favicon.svg
│   ├── numkit_mide.js        # ← copy WASM glue here (optional)
│   └── numkit_mide.wasm      # ← copy WASM binary here (optional)
└── src/
    ├── main.jsx             # React root
    ├── index.css            # Global styles
    ├── App.jsx              # Engine init + loading screen
    ├── engine.js            # WASM / fallback engine abstraction
    ├── interpreter.js       # Built-in JS interpreter (fallback)
    ├── theme.js             # Colour palette + font constants
    ├── components/
    │   └── REPL.jsx     # Main REPL component (all UI)
    └── data/
        ├── help.js          # Help database (80+ functions)
        ├── examples.js      # Example code snippets
        └── cheatsheet.js    # Quick reference data
```

## Architecture Notes

### Engine Interface

Both WASM and fallback engines implement the same interface:

```js
engine.init()            → string   // welcome message
engine.execute(code)     → string   // output text
engine.complete(partial) → string[] // autocomplete suggestions
engine.reset()           → string   // confirmation message
engine.workspace()       → string   // whos output
engine.getVars()         → object   // variable map (fallback only)
engine.getLastPlot()     → object   // plot data after execute()
```

### Adding New Features to C++ Engine

If you add new commands (e.g. `plot`) to the C++ side, you can
communicate structured data back to JS by encoding it in the
output string (e.g. `__PLOT_DATA__:{...json...}`) and parsing it
in `engine.js`.

### GitHub API Rate Limits

The Git Repo Browser uses the public GitHub API (no auth),
which allows 60 requests/hour. For heavier usage, add a personal
access token input in the GitRepoBrowser component.

## Integration with Existing CMake Project

Add this to your top-level `CMakeLists.txt`:

```cmake
option(NUMKIT_BUILD_REPL "Build web REPL" OFF)
if(NUMKIT_BUILD_REPL)
    add_subdirectory(repl)
endif()
```

The existing `repl/CMakeLists.txt` handles Emscripten configuration.
After building, copy the output files as described above.
