# Numkit — lightweight numerical computing engine & MATLAB-compatible runtime

[![License: 0BSD](https://img.shields.io/badge/License-0BSD-blue.svg)](LICENSE)
[![Live Demo](https://img.shields.io/badge/Web_IDE-Live_Demo-success.svg)](https://hotredman.github.io/numkit-demo/)
[![Bugs & Parity](https://img.shields.io/badge/Bugs_%26_Parity-Catalog-orange.svg)](https://hotredman.github.io/numkit-bugs/)
[![npm](https://img.shields.io/badge/npm-numkit-red.svg)](https://www.npmjs.com/package/numkit)

**Numkit** is an ultra-fast, lightweight numerical computing engine and matrix-scripting interpreter written in modern C++17. Designed from the ground up for **AI Agents, WebAssembly sandboxes, and embedded C++ applications**, it delivers MATLAB/Octave compatibility with instant startup and zero external dependencies.

👉 **Try it in your browser:** [https://hotredman.github.io/numkit-demo/](https://hotredman.github.io/numkit-demo/)  
👉 **C++ Library Documentation:** [https://hotredman.github.io/numkit-doxy/](https://hotredman.github.io/numkit-doxy/)  
👉 **Defect & Parity Catalog:** [https://hotredman.github.io/numkit-bugs/](https://hotredman.github.io/numkit-bugs/)  
👉 **LLM / Agent context spec:** [`llms.txt`](ide/public/llms.txt)

---

## 🤖 Built for AI Agents & LLMs

Modern AI coding agents (Claude, Cursor, Antigravity, AutoGen, LangChain) need to execute mathematics, DSP, and numerical analysis reliably. Running Python/Jupyter in Docker is heavy, slow to boot, and token-expensive. NumKit provides the ideal execution runtime for agent tool-calling loops:

* ⚡ **Instant Startup (< 1 ms)** — Zero cold-start latency compared to 300–800 ms for Python / Jupyter runtimes.
* 🪙 **Token-Efficient Syntax** — Native matrix operations (`A \ b`, `[b, a] = butter(4, 0.2);`, `fft(x)`) require 2–3× fewer tokens than Python imports and boilerplate, preserving context window and reducing LLM syntax mistakes.
* 🛡️ **Zero-Install WASM Sandbox** — Runs safely in-process via WebAssembly (Node.js / browser) without requiring Docker containers, root privileges, or local compilers.
* 🔌 **Agent & Tool-Calling Native** — Ready for CLI execution, Model Context Protocol (MCP), and structured JSON output.

### 1-Line Execution for Agents (Zero Install, WASM)

```bash
# Instant calculation via npx (Node >= 16, no native compiler required):
npx -y numkit -e "A = [1 2; 3 4]; disp(A \ [5; 11])"

# Run full scripts:
npx -y numkit script.m
```

### Why AI Agents Choose NumKit over Python

| Feature | Python (NumPy / SciPy) | NumKit |
| :--- | :--- | :--- |
| **Startup Latency** | 300–800 ms (cold start) | **< 1 ms (instant)** |
| **Isolation / Sandbox** | Heavy Docker / gVisor container | **In-process WebAssembly / C++** |
| **Footprint & Deps** | 500 MB+ (Python env, pip, glibc) | **5 MB standalone WASM / Single binary** |
| **Token Efficiency** | Verbose imports & ceremony | **Compact matrix-native DSL** |
| **Deterministic Safety** | Unhandled segfaults in native C extensions | **StackGuard & memory-safe runtime** |

---

## Key Features

- **Matrix-Oriented Scripting**: Column-major multidimensional arrays, complex numbers, cell arrays, structs, and function handles with copy-on-write (COW) memory semantics.
- **Dual Execution Engine**: Fast non-recursive bytecode virtual machine with AST TreeWalker fallback and interactive step-debugger (breakpoints, step over/in/out, paused workspace inspection).
- **Extensive Numerical Toolboxes**:
  - **Math & Linear Algebra**: Arithmetic, trigonometry, special functions, polynomials, LU, QR, Cholesky, SVD, eigenvalue solvers.
  - **Signal & Audio**: FFT/IFFT, digital filtering (`filter`, `filtfilt`, Butterworth/Chebyshev design), spectral analysis, WAV I/O.
  - **Statistics**: Distributions, descriptive statistics, regression, hypothesis tests, PCA.
  - **Image Processing**: Color conversions, spatial filtering, morphology, geometry, image I/O (PNG, BMP, JPEG).
  - **Control Systems & Wavelets**: LTI models, state-space representations, Bode/Nyquist responses, continuous & discrete wavelet transforms.
  - **Optimization & ODE**: Root finding (`fzero`), bounded/unconstrained optimization (`fminbnd`, `fminsearch`), Runge-Kutta ODE solvers.
  - **File I/O**: MAT-file (v5) read/write, CSV, delimited text, audio/image formats.
- **AOT C++ Codegen**: Transpiles scripting code directly to optimized native C++ with SIMD acceleration (Highway).
- **Web & Desktop IDE**: Interactive workspace featuring code editor, variable viewer, 2D/3D figures (SVG & WebGL), and REPL console (available in browser via WebAssembly and as a desktop application via Electron).

---

## Quickstart

### C++ Embedding

```cpp
#include <iostream>
#include <numkit/bundle/standard_engine.hpp>

int main() {
    numkit::StandardEngine engine;

    engine.eval("A = [1, 2; 3, 4];");
    engine.eval("b = [5; 11];");
    engine.eval("x = A \\ b;");
    engine.eval("disp(x);");

    return 0;
}
```

---

## Building

### Prerequisites

- **C++ Engine**: C++17 compatible compiler (GCC 9+, Clang 10+, MSVC 2019+), CMake 3.21+, Ninja (recommended for Linux/macOS)
- **Web & Desktop IDE**: Node.js 18+ and npm
- **WebAssembly Engine** *(optional)*: [Emscripten SDK](https://emscripten.org/) (`EMSDK` environment variable)

---

### Native Engine & CLI

You can build either using automated helper scripts or standard CMake presets:

#### Option A: Quick build via scripts

```bash
# Windows
scripts\windows\engine-build.cmd             # Release (Highway SIMD + Threads)
scripts\windows\tests-run.cmd                # Build & run Google Test suite
scripts\windows\tests-run.cmd --gtest_filter=*Matrix*  # Run specific tests

# Linux / macOS / WSL
./scripts/linux/engine-build.sh              # Release (Highway SIMD + Threads)
./scripts/linux/tests-run.sh                 # Build & run Google Test suite
./scripts/linux/tests-run.sh --gtest_filter=*Matrix*
```
*Script flags:* `--debug` (debug symbols), `--portable` (scalar reference, no SIMD), `--wasm` (WebAssembly).

#### Option B: Standard CMake Presets

```bash
# Configure & build
cmake --preset windows-release               # On Linux: cmake --preset linux-release
cmake --build --preset windows-release

# Available presets: windows-release, windows-debug, windows-portable,
#                    linux-release,   linux-debug,   linux-portable, wasm-release
```

#### Output Artifacts

Binaries are located in `build/<os>/<config>/`:
- `build/<os>/<config>/apps/numkit/numkit_repl` — Interactive MATLAB-compatible REPL and CLI runner (`numkit_repl script.m`).
- `build/<os>/<config>/apps/numkit_codegen/numkit_codegen` — C++ AOT code generator.
- `build/<os>/<config>/tests/gtest/numkit_gtest` — Google Test regression runner.

---

### Web & Desktop IDE

#### Web IDE (WebAssembly + Vite)
```bash
scripts\windows\web-build.cmd    # Windows (outputs static bundle to deploy/web/)
./scripts/linux/web-build.sh     # Linux / macOS
```

#### Desktop Application (Electron)
```bash
scripts\windows\desktop-build.cmd   # Packages Windows portable .exe
./scripts/linux/desktop-build.sh    # Packages Linux AppImage and unpackaged dir
```

For development servers, documentation generators, and deployment workflows, see [`scripts/README.md`](scripts/README.md).

---

## Repository Structure

- `src/` — Core C++ interpreter, bytecode VM, value types, and domain toolboxes.
- `ide/` — React + Vite IDE frontend and Electron desktop shell.
- `wasm/` — Emscripten bindings and WASM build pipeline.
- `examples/` — MATLAB-compatible demonstration scripts across all toolboxes.
- `tests/` — Comprehensive C++ unit and integration test suite.
- `tools/` — Parity testing harness, benchmarks, and validation utilities.
- `bugs/` — Structured defect catalog and MATLAB parity tracking ([live catalog](https://hotredman.github.io/numkit-bugs/)).
- `dev-docs/` — Architecture documentation, memory records, and API guidelines.

---

## License

Numkit is open-source software released under the **[BSD Zero Clause License (0BSD)](LICENSE)**.

You are free to use, copy, modify, and distribute this software for any purpose, commercial or noncommercial, with or without fee, with no attribution required. See [NOTICE](NOTICE) for third-party component licenses.
