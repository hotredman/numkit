# Matrix-Scripting Interpreter in C++

A lightweight interpreter for a matrix-oriented scripting language — scalars and matrices, complex numbers, cell arrays, structs, function handles, plotting, DSP and fitting libraries. Written in modern C++17 with column-major storage and copy-on-write value semantics. Ships with a browser-based IDE (mIDE) compiled to WebAssembly. Designed to embed scientific scripting into C++ applications with full control over memory allocation, I/O, and extensibility.

<a href="https://numkit.github.io/numkit-m/">
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="brand/numkit-mide-logo-dark.svg">
    <img src="brand/numkit-mide-logo-light.svg" alt="Try mIDE in browser" width="280">
  </picture>
</a>

**[Launch mIDE in Browser →](https://numkit.github.io/numkit-m/)**

---

## Web IDE

mIDE is built with React + Vite and runs the C++ engine via WebAssembly:

- **Syntax highlighting** — keywords, builtins, constants, strings, comments
- **Dark / Light theme** — single-source theming via React Context
- **File browser** — local virtual FS, bundled examples (80 scripts), GitHub repo browser
- **Multi-tab editor** — context menu, scroll arrows, rename, close all/others
- **Interactive figures** — SVG-rendered plots with resize, subplot, polar, imagesc
- **Console** — command history, tab completion, inline help
- **Workspace inspector** — live variable viewer with types and previews
- **Debugger** — breakpoints, step over/into/out, continue, expression evaluation in paused context

---

## Features

### Language Support

| Feature | Status |
|---|---|
| Scalar and matrix arithmetic | Done |
| Complex numbers (`3+4i`, `2.5j`) | Done |
| Element-wise operators (`.*`, `./`, `.^`) | Done |
| Matrix multiplication (`*`) | Done |
| Conjugate transpose (`'`) and transpose (`.'`) | Done |
| Comparison operators (`==`, `~=`, `<`, `>`, `<=`, `>=`) | Done |
| Logical operators (`&`, `\|`, `~`, `&&`, `\|\|`) | Done |
| Short-circuit evaluation (`&&`, `\|\|`) | Done |
| Colon expressions (`1:10`, `0:0.5:5`, `10:-1:1`) | Done |
| String literals (single and double quoted) | Done |
| String concatenation (`['Hello' ' ' 'World']`) | Done |
| Matrix literals (`[1 2; 3 4]`) | Done |
| Cell arrays (`{1, 'hello'; [1 2], true}`) | Done |
| Structs and nested structs | Done |
| Function handles (`@func`, `@(x) x^2`) | Done |
| Closures with environment capture | Done |
| `if` / `elseif` / `else` / `end` | Done |
| `for` / `end` (numeric, char, cell, logical) | Done |
| `while` / `end` | Done |
| `switch` / `case` / `otherwise` / `end` | Done |
| `case {1, 2, 3}` (cell matching in switch) | Done |
| `break`, `continue`, `return` | Done |
| `try` / `catch` / `end` | Done |
| `global` / `persistent` | Done |
| User-defined functions with `nargin`/`nargout` | Done |
| Multiple return values (`[a, b] = func(...)`) | Done |
| Recursive functions | Done |
| Anonymous functions with closures | Done |
| `end` keyword in indexing (`A(end)`, `A(end-1)`) | Done |
| Linear and subscript indexing | Done |
| Logical indexing | Done |
| Element deletion (`A(idx) = []`) | Done |
| 2D and 3D array support | Done |
| Command-style syntax (`clear all`, `grid on`) | Done |
| Implicit semicolon suppression | Done |
| Line continuation (`...`) | Done |
| Comments (`%`) and block comments (`%{ %}`) | Done |

### Library Surface

numkit ships a substantial function surface organized into namespaced
libraries that mirror the structure of MATLAB / Octave toolboxes:

| Library | Coverage |
|---|---|
| **Builtin** | Language fundamentals, math (arithmetic, trig, exp/log, special, polynomials, RNG, interpolation), workspace, error handling |
| **Signal** | FFT family, filter design / analysis / implementation, windows, spectral estimation, transforms, time-frequency, multirate, smoothing, vibration analysis |
| **Statistics** | Descriptive stats, distributions, hypothesis tests, regression, clustering, dimensionality reduction, NaN-aware reductions |
| **Image** | I/O, type & color conversion, filtering, morphology, segmentation, registration, geometric transforms, deblurring, region/object analysis |
| **Communications** | Modulation, source / channel coding, interleaving, equalization, RF impairments, performance analysis |
| **Control** | LTI models, state-space, conversion, interconnections, time / frequency response, stability, PID |
| **Wavelet** | CWT / DWT (1-D / 2-D / 3-D), MODWT, denoising, filter banks, lifting, decomposition trees |
| **Graphics** | Line / polar / contour / vector / surface / volume / geographic plots; figure & subplot management |
| **IO** | Low-level file I/O, CSV / dlm / readtable, spreadsheets, workspace save/load |
| **Optimization** | `fzero`, `fminbnd`, `fminsearch` (constrained / global solvers landing) |
| **Fitting** | Splines |

Live function-by-function status, native runtime, and side-by-side
performance against MATLAB R2025b and Octave 11.x is tracked in
**[PROGRESS.md](PROGRESS.md)**.

Known issues, behavioural deviations, and open bugs are tracked in
**[BUGS.md](BUGS.md)**.

### Debugger

mIDE includes a full-featured debugger with pause/resume support:

- **Breakpoints** — click gutter to set; supported on all statement lines including `end`
- **Step over / Step into / Step out / Continue** — standard stepping controls
- **Expression evaluation** — evaluate arbitrary code in the paused context (access local variables, call functions, plot)
- **Workspace inspection** — variables from the current scope displayed during pause
- **Call stack** — full call stack with function names and line numbers
- **Figures during debug** — `plot()`, `figure()`, `close()` work during pause and eval

---

## Architecture

```
Source Code → Lexer → Tokens → Parser → AST → Compiler → Bytecode → VM (execute)
                                          │                            ↓
                                          └→ TreeWalker (fallback)   DebugController
                                                                       ↓
                                                                   DebugSession
                                                                  (pause/resume/eval)
```

All classes live in `namespace numkit`.

| Module | Class | Responsibility |
|---|---|---|
| **Lexer** | `Lexer` | Tokenization with implicit comma insertion inside `[]` |
| **Parser** | `Parser` | Recursive-descent parser producing AST |
| **AST** | `Ast` | Node types, `unique_ptr`-based tree with `cloneNode()` |
| **Compiler** | `Compiler`, `Bytecode` | AST → bytecode compiler with source maps |
| **VM** | `VM` | Non-recursive bytecode interpreter with explicit call stack |
| **TreeWalker** | `TreeWalker` | AST-walking interpreter (automatic fallback) |
| **Engine** | `Engine` | Dual-backend orchestrator, function registry, variable store |
| **Debugger** | `Debugger` | Breakpoints, stepping, call stack, debug observer protocol |
| **DebugSession** | `DebugSession` | Pausable execution, expression eval in context, VM state save/restore |
| **Value** | `Value` | Copy-on-write value system (double, complex, logical, char, cell, struct, function_handle) |
| **Environment** | `Environment` | Scoped variable storage with global store |
| **Memory** | `std::pmr::memory_resource*` | Pluggable heap (passed to Engine ctor; embedders subclass `std::pmr::memory_resource` for custom policy) |
| **BuiltinLibrary** | `BuiltinLibrary` | Base layer: language fundamentals, math, programming primitives |
| **SignalLibrary** | `SignalLibrary` | DSP: FFT, filter design / analysis / implementation, windows, spectral, transforms, multirate |
| **StatsLibrary** | `StatsLibrary` | Statistics: descriptive, distributions, hypothesis tests, regression, clustering |
| **ImageLibrary** | `ImageLibrary` | Image processing: I/O, filtering, morphology, segmentation, color, transforms |
| **CommLibrary** | `CommLibrary` | Communications: modulation, coding, equalization, channel impairments |
| **ControlLibrary** | `ControlLibrary` | Control systems: LTI models, state-space, frequency response, stability |
| **WaveletLibrary** | `WaveletLibrary` | Wavelets: CWT / DWT, MODWT, denoising, filter banks |
| **GraphicsLibrary** | `GraphicsLibrary` | Plot commands, figure / subplot management |
| **IoLibrary** | `IoLibrary` | File I/O: low-level, CSV, spreadsheets, workspace save/load |
| **OptimLibrary** | `OptimLibrary` | Optimization: `fzero`, `fminbnd`, `fminsearch` |
| **FigureManager** | `FigureManager` | Plot state management with subplot/axes support |

### Key Design Decisions

- **Dual backend** — bytecode VM for performance, TreeWalker as automatic fallback
- **Non-recursive VM** — explicit `CallFrame` stack on heap enables pause/resume for debugging
- **Copy-on-Write (COW)** for matrix data — efficient passing without deep copies
- **Column-major storage** — standard numerical memory layout
- **`unique_ptr<ASTNode>`** — zero-overhead AST ownership, `shared_ptr` only for function bodies stored in the engine
- **RAII guards** — `IndexContextGuard` and `RecursionGuard` ensure exception safety
- **Pluggable allocator** — track memory, use custom pools, or integrate with your application's allocator
- **Non-copyable, non-movable `Engine`** — prevents dangling references in registered lambdas
- **Environment snapshots** — anonymous functions capture variables by value at creation time
- **AxesState / FigureState** — per-axes configuration supports subplot grids with independent settings
- **Figure output through `outputFunc`** — no `std::cout` dependency; all markers route through the engine's output callback
- **Constants protection** — `clear all` reinstalls `pi`, `eps`, `inf`, `nan`, `true`, `false`, `i`, `j`

---

## Building

### Requirements

- C++17 compiler (GCC 8+, Clang 7+, MSVC 2019+)
- CMake 3.21+

### Build

Via CMake presets (see `CMakePresets.json`):

```bash
cmake --preset=portable         # reference build, no optimizations
cmake --build --preset=portable
```

Or use the wrapper scripts: `./build.sh` (Linux/macOS) or `build.bat` (Windows).

### Run Tests

```bash
ctest --preset=portable
# 6382 / 6387 tests passing (1 skipped, 4 disabled)
```

### Build Web IDE (WebAssembly)

```bash
# Requires Emscripten SDK with EMSDK env var set
cmake --preset=browser
cmake --build --preset=browser
cd ide
npm install
npm run build
```

Or: `./build.sh --wasm` / `build.bat --wasm`.

---

## Usage

### Basic Embedding

```c++
#include <numkit/core/engine.hpp>
#include <numkit/builtin/library.hpp>

int main()
{
    numkit::Engine engine;
    numkit::BuiltinLibrary::install(engine);

    engine.eval("x = [1 2 3; 4 5 6]");
    engine.eval("disp(size(x))");
    engine.eval("disp(sum(x))");

    return 0;
}
```

### Custom heap (memory_resource)

Subclass `std::pmr::memory_resource` and pass it to the Engine. The
resource must outlive the Engine (and every Value it produces).

```c++
#include <memory_resource>

struct CountingHeap : public std::pmr::memory_resource
{
    size_t totalAllocated = 0;
protected:
    void *do_allocate(size_t n, size_t /*align*/) override {
        totalAllocated += n;
        return ::operator new(n);
    }
    void do_deallocate(void *p, size_t /*n*/, size_t /*align*/) override {
        ::operator delete(p);
    }
    bool do_is_equal(const memory_resource &o) const noexcept override {
        return this == &o;
    }
};

CountingHeap heap;
numkit::Engine engine(&heap);
numkit::BuiltinLibrary::install(engine);
engine.eval("A = rand(100, 100);");
std::cout << "Memory used: " << heap.totalAllocated << " bytes\n";
```

### C++ <-> M Data Exchange

```c++
using numkit::Engine;
using numkit::Value;
using numkit::BuiltinLibrary;

Engine engine;
BuiltinLibrary::install(engine);

// C++ -> M
auto& alloc = engine.allocator();
engine.setVariable("radius", Value::scalar(5.0, &alloc));
engine.eval("area = pi * radius^2;");

// M -> C++
auto* area = engine.getVariable("area");
if (area)
    std::cout << "Area = " << area->toScalar() << "\n";
```

### Registering Custom Functions

```c++
using numkit::Value;

engine.registerFunction("myfunc",
    [&engine](const std::vector<Value>& args) -> std::vector<Value> {
        auto* alloc = &engine.allocator();
        double x = args[0].toScalar();
        double y = args[1].toScalar();
        return {Value::scalar(x * x + y * y, alloc)};
    });

engine.eval("disp(myfunc(3, 4))");  // 25
```

### Plotting

```c++
// FigureManager collects plot data; output goes through engine's outputFunc
numkit::Engine engine;
numkit::BuiltinLibrary::install(engine);

engine.setOutputFunc([](const std::string &s) {
    // Parse __FIGURE_DATA__ markers from output for your renderer
    std::cout << s;
});

engine.eval(R"(
    x = linspace(0, 2*pi, 100);
    figure(1);
    plot(x, sin(x), 'b-');
    hold on;
    plot(x, cos(x), 'r--');
    title('Trigonometric Functions');
    legend('sin', 'cos');
    grid on;
)");
```

### Signal Processing

```octave
% Design a low-pass Butterworth filter
[b, a] = butter(4, 0.3);
y = filter(b, a, x);

% FFT analysis
X = fft(x);
f = (0:length(X)-1) / length(X);
plot(f, abs(X));

% Spectrogram
spectrogram(x, hamming(256), 128, 512, fs);
```

### Debugger (C++ API)

```c++
using numkit::Engine;
using numkit::BuiltinLibrary;
using numkit::DebugSession;
using numkit::DebugAction;

Engine engine;
BuiltinLibrary::install(engine);

DebugSession session(engine);
session.setBreakpoints({3, 7});

auto status = session.start(code);
// status == ExecStatus::Paused

auto snap = session.snapshot();
// snap.line, snap.functionName, snap.variables, snap.callStack

std::string result = session.eval("x + 1");  // evaluate in paused context

status = session.resume(DebugAction::StepOver);
```

### Complex Numbers

```octave
z = 3 + 4i;
disp(abs(z))      % 5
disp(real(z))     % 3
disp(imag(z))     % 4
disp(conj(z))     % 3 - 4i
disp(angle(z))    % 0.9273
```

### Closures

```octave
function h = make_adder(n)
    h = @(x) x + n;
end

add5 = make_adder(5);
disp(add5(10))  % 15
disp(add5(20))  % 25
```

### Structs

```octave
config.server.host = 'localhost';
config.server.port = 8080;
config.db.pool.min = 5;
config.db.pool.max = 20;

disp(config.server.host)    % localhost
disp(config.db.pool.max)    % 20
disp(fieldnames(config))    % {'db', 'server'}
```

---

## Limitations

- **Linear algebra** — basic factorisations only (`mldivide`, `mrdivide`,
  `inv`, `det`, named-fn variants); BLAS/LAPACK-grade `eig` / `svd` /
  `qr` / `chol` are work-in-progress. Tracked under `## Linear Algebra`
  in [PROGRESS.md](PROGRESS.md).
- **Sparse matrices** — no sparse value type yet.
- **Datetime / Tables / Categorical** — datatype scaffolding only,
  not feature-complete.
- **OOP (`classdef`)** — not supported.
- **GUI** — no `uifigure` / `uicontrol` / dialog functions.
- **Figure export** — no `saveas` / `print` to file (figures emit JSON
  for the IDE's SVG renderer).

Behavioural deviations from MATLAB R2025b on individual functions are
tracked in **[BUGS.md](BUGS.md)** alongside their fix queue. Function-
level coverage is in **[PROGRESS.md](PROGRESS.md)**.

---

## Project Structure

```
core/                                 # Runtime: parser → AST → VM / TreeWalker
    include/numkit/core/              # Public headers (namespace numkit)
        engine.hpp · value.hpp · environment.hpp · vm.hpp · compiler.hpp
        tree_walker.hpp · ast.hpp · lexer.hpp · parser.hpp
        debugger.hpp · debug_session.hpp · figure_manager.hpp
        scratch.hpp · vfs.hpp · types.hpp · branding.hpp
    src/                              # Implementations matching the headers
    tests/                            # Frontend / backend / debugger tests

libs/                                 # Domain libraries (one per H2 in PROGRESS.md)
    builtin/                          # Base layer — language fundamentals + math
        include/numkit/builtin/{language,math,programming}/
        src/{language,math,programming}/
    signal/                           # DSP toolbox (12 sub-domains)
    stats/                            # Statistics
    image/                            # Image processing
    comm/                             # Communications
    control/                          # Control systems
    wavelet/                          # Wavelet transforms
    graphics/                         # Plotting commands
    io/                               # File I/O
    optim/                            # Optimization (fzero / fminbnd / fminsearch)
    <each lib>/{include,src,tests}/   # Same layout per library

tests/                                # Top-level integration & cross-cutting tests

wasm/                                 # WebAssembly REPL bindings (Emscripten)
ide/                                  # mIDE — React + Vite frontend
    src/components/                   # IDE.jsx · Console · Figures · FileBrowser · …
    src/engine.js                     # WASM engine wrapper
    desktop/                          # Electron shell
    public/examples/                  # 80 example .m scripts

tools/parity/                         # Parity harness — runs MATLAB / Octave / numkit
    run_parity.py                     # Per-spec runner; updates PROGRESS.md rows
    diff_local_ref.py                 # MATLAB-doc TOC vs PROGRESS.md gap report
    specs/                            # JSON spec per function (input setup + tol)

PROGRESS.md                           # Live function-by-function parity map
BUGS.md                               # Behavioural deviations + fix queue
NAMESPACE_DESIGN.md                   # Lib namespace conventions
COORDINATION.md                       # Multi-session protocol (currently dormant)
```

## License

Apache License 2.0. See [LICENSE](LICENSE) and [NOTICE](NOTICE) for details.
