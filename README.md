# Numkit

[![License: 0BSD](https://img.shields.io/badge/License-0BSD-blue.svg)](LICENSE)
[![Live Demo](https://img.shields.io/badge/Web_IDE-Live_Demo-success.svg)](https://hotredman.github.io/numkit-demo/)

**Numkit** is a lightweight matrix-scripting language interpreter and numerical computing library written in modern C++17. It provides high MATLAB/Octave compatibility, copy-on-write tensor semantics, an AOT C++ code generator, and an interactive Web/Desktop IDE.

👉 **Try it in your browser:** [https://hotredman.github.io/numkit-demo/](https://hotredman.github.io/numkit-demo/)

Designed both for embedding scientific scripting into C++ applications and for standalone engineering workflows.

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

### Requirements

- C++17 compatible compiler (GCC 9+, Clang 10+, MSVC 2019+)
- CMake 3.21+
- Node.js 18+ (for Web/Desktop IDE)

### Build Native Engine

```bash
# Configure and build
cmake --preset=portable
cmake --build --preset=portable

# Run tests
ctest --preset=portable
```

### Build Web IDE (WebAssembly)

```bash
# Build WASM backend and frontend bundle
scripts/web-build.bat        # Windows
# or ./scripts/web-build.sh   # Linux/macOS
```

---

## Repository Structure

- `src/` — Core C++ interpreter, bytecode VM, value types, and domain toolboxes.
- `ide/` — React + Vite IDE frontend and Electron desktop shell.
- `wasm/` — Emscripten bindings and WASM build pipeline.
- `examples/` — MATLAB-compatible demonstration scripts across all toolboxes.
- `tests/` — Comprehensive C++ unit and integration test suite.
- `tools/` — Parity testing harness, benchmarks, and validation utilities.
- `dev-docs/` — Architecture documentation, memory records, and API guidelines.

---

## License

Numkit is open-source software released under the **[BSD Zero Clause License (0BSD)](LICENSE)**.

You are free to use, copy, modify, and distribute this software for any purpose, commercial or noncommercial, with or without fee, with no attribution required. See [NOTICE](NOTICE) for third-party component licenses.
