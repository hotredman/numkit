# MLab — Embeddable MATLAB Interpreter in C++ written with Claude Opus

A lightweight, embeddable interpreter for a substantial subset of the MATLAB language, written in modern C++17. Includes a browser-based IDE compiled to WebAssembly. Designed for embedding MATLAB-like scripting capabilities into C++ applications with full control over memory allocation, I/O, and extensibility.

[![Try Online](https://img.shields.io/badge/Try%20Online-MLab%20IDE-blue?style=for-the-badge&logo=webassembly)](https://skynetjudgmentday.github.io/mlab/)

**[Launch MLab IDE in Browser](https://skynetjudgmentday.github.io/mlab/)**

---

## Web IDE

MLab includes a full-featured browser IDE built with React + Vite, running the C++ engine via WebAssembly:

- **Syntax highlighting** — keywords, builtins, constants, strings, comments
- **Dark / Light theme** — single-source theming via React Context
- **File browser** — local virtual FS, bundled examples (62 scripts), GitHub repo browser
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

### Built-in Functions

#### Math
`sqrt`, `abs`, `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`,
`exp`, `log`, `log2`, `log10`, `floor`, `ceil`, `round`, `fix`,
`mod`, `rem`, `sign`, `max`, `min`, `sum`, `prod`, `mean`, `cumsum`,
`linspace`, `logspace`, `rand`, `randn`, `deg2rad`, `rad2deg`

#### Matrix
`zeros`, `ones`, `eye`, `size`, `length`, `numel`, `ndims`, `reshape`,
`transpose`, `diag`, `sort`, `find`, `horzcat`, `vertcat`,
`cross`, `dot`, `meshgrid`

#### I/O
`disp`, `fprintf`, `sprintf`, `error`, `warning`

#### Type Queries
`double`, `logical`, `char`, `isnumeric`, `islogical`, `ischar`,
`iscell`, `isstruct`, `isempty`, `isscalar`, `isreal`, `isnan`, `isinf`

#### String
`num2str`, `str2num`, `str2double`, `strcmp`, `strcmpi`,
`upper`, `lower`, `strtrim`, `strsplit`, `strcat`

#### Cell & Struct
`struct`, `fieldnames`, `isfield`, `rmfield`, `cell`

#### Complex
`real`, `imag`, `conj`, `complex`, `angle`

#### Workspace
`clear`, `who`, `whos`, `which`, `exist`, `class`

#### Constants
`pi`, `eps`, `inf`, `Inf`, `nan`, `NaN`, `i`, `j`, `true`, `false`

### Signal Processing (DSP Library)

#### Transforms
`fft`, `ifft`, `fftshift`, `ifftshift`, `hilbert`, `unwrap`, `envelope`

#### Filtering
`filter`, `filtfilt`, `conv`, `deconv`, `xcorr`

#### Filter Design
`butter`, `fir1`, `freqz`

#### Windows
`hamming`, `hanning`, `blackman`, `kaiser`, `rectwin`, `bartlett`

#### Spectral Analysis
`periodogram`, `pwelch`, `spectrogram`

#### Resampling
`downsample`, `upsample`, `decimate`, `resample`

#### Utility
`nextpow2`

### Interpolation & Fitting (Fit Library)

`interp1` (linear, nearest, spline, pchip), `spline`, `pchip`,
`polyfit`, `polyval`, `trapz`

### Plotting

MLab includes a figure management system that outputs plot data as JSON, rendered as SVG in the Web IDE.

#### Plot Types
`plot`, `bar`, `scatter`, `stem`, `stairs`, `hist`, `imagesc`,
`polarplot`, `semilogx`, `semilogy`, `loglog`

#### Figure Management
`figure`, `close`, `clf`, `subplot`

#### Plot Configuration
`title`, `xlabel`, `ylabel`, `legend`, `xlim`, `ylim`, `clim`, `colormap`,
`grid` (on/off/minor), `hold` (on/off), `axis` (equal/tight/ij/xy)

#### Polar Configuration
`rlim`, `thetalim`, `thetadir`, `thetazero`

#### Style
Style strings (`'r--o'`, `'b:'`, `'g-.'`) and name-value pairs (`'LineWidth'`, `'MarkerSize'`)

### Debugger

The IDE includes a MATLAB-like debugger with full pause/resume support:

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

| Module | Files | Responsibility |
|---|---|---|
| **Lexer** | `MLabLexer` | Tokenization with implicit comma insertion inside `[]` |
| **Parser** | `MLabParser` | Recursive descent parser producing AST |
| **AST** | `MLabAst` | Node types, `unique_ptr`-based tree with `cloneNode()` |
| **Compiler** | `MLabCompiler`, `MLabBytecode` | AST → bytecode compiler with source maps |
| **VM** | `MLabVM` | Non-recursive bytecode interpreter with explicit call stack |
| **TreeWalker** | `MLabTreeWalker` | AST-walking interpreter (automatic fallback) |
| **Engine** | `MLabEngine` | Dual-backend orchestrator, function registry, variable store |
| **Debugger** | `MLabDebugger` | Breakpoints, stepping, call stack, debug observer protocol |
| **DebugSession** | `MLabDebugSession` | Pausable execution, expression eval in context, VM state save/restore |
| **Value** | `MLabValue` | Copy-on-write value system (double, complex, logical, char, cell, struct, function_handle) |
| **Environment** | `MLabEnvironment` | Scoped variable storage with global store |
| **Allocator** | `MLabAllocator` | Pluggable memory allocator |
| **StdLibrary** | `MLabStdLibrary` | Math, matrix, I/O, string, type built-ins |
| **DspLibrary** | `MLabDspLibrary` | Signal processing: FFT, filtering, windows, spectral analysis |
| **FitLibrary** | `MLabFitLibrary` | Interpolation, polynomial fitting, integration |
| **PltLibrary** | `MLabPltLibrary` | Plot commands, figure/close/clf/subplot |
| **FigureManager** | `MLabFigureManager` | Plot state management with subplot/axes support |

### Key Design Decisions

- **Dual backend** — bytecode VM for performance, TreeWalker as automatic fallback
- **Non-recursive VM** — explicit `CallFrame` stack on heap enables pause/resume for debugging
- **Copy-on-Write (COW)** for matrix data — efficient passing without deep copies
- **Column-major storage** — matches MATLAB memory layout
- **`unique_ptr<ASTNode>`** — zero-overhead AST ownership, `shared_ptr` only for function bodies stored in the engine
- **RAII guards** — `IndexContextGuard` and `RecursionGuard` ensure exception safety
- **Pluggable allocator** — track memory, use custom pools, or integrate with your application's allocator
- **Non-copyable, non-movable `Engine`** — prevents dangling references in registered lambdas
- **Environment snapshots** — anonymous functions capture variables by value at creation time (MATLAB semantics)
- **AxesState / FigureState** — per-axes configuration supports subplot grids with independent settings
- **Figure output through `outputFunc`** — no `std::cout` dependency; all markers route through the engine's output callback
- **Constants protection** — `clear all` reinstalls `pi`, `eps`, `inf`, `nan`, `true`, `false`, `i`, `j`

---

## Building

### Requirements

- C++17 compiler (GCC 8+, Clang 7+, MSVC 2019+)
- CMake 3.14+

### Build

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

### Run Tests

```bash
cd build
ctest --output-on-failure
# 2383 tests across 121 suites
```

### Build Web IDE (WebAssembly)

```bash
# Requires Emscripten SDK
cd wasm && mkdir build && cd build
emcmake cmake ../..
emmake make -j$(nproc)
cd ../../ide
npm install
npm run build
```

---

## Usage

### Basic Embedding

```c++
#include "MLabEngine.hpp"
#include "MLabStdLibrary.hpp"

int main()
{
    mlab::Engine engine;
    mlab::StdLibrary::install(engine);

    engine.eval("x = [1 2 3; 4 5 6]");
    engine.eval("disp(size(x))");
    engine.eval("disp(sum(x))");

    return 0;
}
```

### Custom Allocator

```c++
mlab::Engine engine;

size_t totalAllocated = 0;
engine.setAllocator({
    [&](size_t n) -> void* {
        totalAllocated += n;
        return ::operator new(n);
    },
    [&](void* p, size_t n) {
        totalAllocated -= n;
        ::operator delete(p);
    }
});

mlab::StdLibrary::install(engine);
engine.eval("A = rand(100, 100);");
std::cout << "Memory used: " << totalAllocated << " bytes\n";
```

### C++ <-> MATLAB Data Exchange

```c++
mlab::Engine engine;
mlab::StdLibrary::install(engine);

// C++ -> MATLAB
auto& alloc = engine.allocator();
engine.setVariable("radius", mlab::MValue::scalar(5.0, &alloc));
engine.eval("area = pi * radius^2;");

// MATLAB -> C++
auto* area = engine.getVariable("area");
if (area)
    std::cout << "Area = " << area->toScalar() << "\n";
```

### Registering Custom Functions

```c++
engine.registerFunction("myfunc",
    [&engine](const std::vector<mlab::MValue>& args) -> std::vector<mlab::MValue> {
        auto* alloc = &engine.allocator();
        double x = args[0].toScalar();
        double y = args[1].toScalar();
        return {mlab::MValue::scalar(x * x + y * y, alloc)};
    });

engine.eval("disp(myfunc(3, 4))");  // 25
```

### Plotting (Embeddable)

```c++
// FigureManager collects plot data; output goes through engine's outputFunc
mlab::Engine engine;
mlab::StdLibrary::install(engine);

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

```matlab
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
mlab::Engine engine;
mlab::StdLibrary::install(engine);

mlab::DebugSession session(engine);
session.setBreakpoints({3, 7});

auto status = session.start(code);
// status == ExecStatus::Paused

auto snap = session.snapshot();
// snap.line, snap.functionName, snap.variables, snap.callStack

std::string result = session.eval("x + 1");  // evaluate in paused context

status = session.resume(mlab::DebugAction::StepOver);
```

### Complex Numbers

```matlab
z = 3 + 4i;
disp(abs(z))      % 5
disp(real(z))     % 3
disp(imag(z))     % 4
disp(conj(z))     % 3 - 4i
disp(angle(z))    % 0.9273
```

### Closures

```matlab
function h = make_adder(n)
    h = @(x) x + n;
end

add5 = make_adder(5);
disp(add5(10))  % 15
disp(add5(20))  % 25
```

### Structs

```matlab
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

- No linear algebra beyond `cross`/`dot` (`det`, `inv`, `eig`, `norm` not implemented)
- No sparse matrix support
- No object-oriented programming (classdef)
- No Simulink or toolbox functions
- Limited `fprintf`/`sprintf` formatting
- No file I/O (`fopen`, `fread`, etc.)
- No regular expressions (`regexp`)
- No GUI functions
- Matrix left division (`\`) only for scalars
- Matrix power (`^`) only for scalars
- 3D plot functions registered as no-ops (`surf`, `mesh`, `contour`, `scatter3`)
- No `saveas` / figure export

---

## Project Structure

```
include/                        # Public headers
    MLabEngine.hpp              # Dual-backend engine
    MLabVM.hpp                  # Bytecode virtual machine
    MLabCompiler.hpp            # AST -> bytecode compiler
    MLabBytecode.hpp            # Bytecode instruction set
    MLabTreeWalker.hpp          # AST interpreter (fallback)
    MLabDebugger.hpp            # Breakpoints, stepping, debug controller
    MLabDebugSession.hpp        # Pausable debug execution
    MLabValue.hpp               # Copy-on-write value system
    MLabAst.hpp                 # AST node types
    MLabLexer.hpp               # Tokenizer
    MLabParser.hpp              # Recursive descent parser
    MLabEnvironment.hpp         # Scoped variable storage
    MLabAllocator.hpp           # Pluggable allocator
    MLabFigureManager.hpp       # Plot state management
    MLabStdLibrary.hpp          # Standard library
    MLabDspLibrary.hpp          # Signal processing library
    MLabFitLibrary.hpp          # Interpolation / fitting library
    MLabPltLibrary.hpp          # Plot library
    MLabTypes.hpp               # Shared types (CallContext, Span)
src/
    MLabEngine.cpp              # Engine orchestration
    MLabVM.cpp                  # VM dispatch loop
    MLabCompiler.cpp            # Bytecode compilation
    MLabTreeWalker.cpp          # Tree-walking interpreter
    MLabDebugger.cpp            # Debug controller logic
    MLabDebugSession.cpp        # Debug session (pause/resume/eval)
    MLabValue.cpp               # Value operations
    MLabLexer.cpp               # Lexer
    MLabParser.cpp              # Parser
    MLabEnvironment.cpp         # Environment
    MLabAllocator.cpp           # Allocator
    MLabAst.cpp                 # AST utilities
    stdlib/                     # Standard library (10 files)
    dsplib/                     # DSP library (9 files)
    fitlib/                     # Fit library (2 files)
    pltlib/                     # Plot library (1 file)
tests/                          # 2383 tests, 121 suites
    core/                       # Lexer, parser, command syntax
    stdlib/                     # Language + standard library
    dsplib/                     # Signal processing
    fitlib/                     # Interpolation
    pltlib/                     # Figures and plotting
    backend/                    # VM, TreeWalker, benchmarks
    diagnostics/                # Debugger, error messages
wasm/                           # WebAssembly build
    src/repl_bindings.cpp       # Emscripten bindings
ide/                            # Browser IDE (React + Vite)
    src/
        components/
            MLabREPL.jsx        # Main IDE layout
            Console.jsx         # Command console
            Figures.jsx         # SVG plot renderer (D3)
            FileBrowser.jsx     # Local / Examples / GitHub browser
            SyntaxEditor.jsx    # MATLAB syntax highlighting
            Workspace.jsx       # Variable inspector
            Reference.jsx       # Cheat sheet
        engine.js               # WASM engine wrapper
        vfs.js                  # Virtual filesystem (IndexedDB)
        theme.jsx               # Dark / Light theme
    public/examples/            # 62 example .m scripts
```

## License

Apache License 2.0. See [LICENSE](LICENSE) and [NOTICE](NOTICE) for details.
