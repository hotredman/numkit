# MLab — Embeddable MATLAB Interpreter in C++ written with Claude Opus

A lightweight, embeddable interpreter for a substantial subset of the MATLAB language, written in modern C++17. Includes a browser-based IDE compiled to WebAssembly. Designed for embedding MATLAB-like scripting capabilities into C++ applications with full control over memory allocation, I/O, and extensibility.

[![Try Online](https://img.shields.io/badge/Try%20Online-MLab%20IDE-blue?style=for-the-badge&logo=webassembly)](https://skynetjudgmentday.github.io/mlab/)

**[▶ Launch MLab IDE in Browser](https://skynetjudgmentday.github.io/mlab/)**

---

## Web IDE

MLab includes a full-featured browser IDE built with React + Vite, running the C++ engine via WebAssembly:

- **Syntax highlighting** — keywords, builtins, constants, strings, comments
- **Dark / Light theme** — single-source theming via React Context
- **File browser** — local virtual FS, bundled examples, GitHub repo browser
- **Multi-tab editor** — context menu, scroll arrows, rename, close all/others
- **Interactive figures** — SVG-rendered plots with resize support
- **Console** — command history, tab completion, inline help
- **Workspace inspector** — live variable viewer with types and previews

---

## Features

### Language Support

| Feature | Status |
|---|---|
| Scalar and matrix arithmetic | ✅ |
| Complex numbers (`3+4i`, `2.5j`) | ✅ |
| Element-wise operators (`.*`, `./`, `.^`) | ✅ |
| Matrix multiplication (`*`) | ✅ |
| Conjugate transpose (`'`) and transpose (`.'`) | ✅ |
| Comparison operators (`==`, `~=`, `<`, `>`, `<=`, `>=`) | ✅ |
| Logical operators (`&`, `\|`, `~`, `&&`, `\|\|`) | ✅ |
| Short-circuit evaluation (`&&`, `\|\|`) | ✅ |
| Colon expressions (`1:10`, `0:0.5:5`, `10:-1:1`) | ✅ |
| String literals (single and double quoted) | ✅ |
| String concatenation (`['Hello' ' ' 'World']`) | ✅ |
| Matrix literals (`[1 2; 3 4]`) | ✅ |
| Cell arrays (`{1, 'hello'; [1 2], true}`) | ✅ |
| Structs and nested structs | ✅ |
| Function handles (`@func`, `@(x) x^2`) | ✅ |
| Closures with environment capture | ✅ |
| `if` / `elseif` / `else` / `end` | ✅ |
| `for` / `end` (numeric, char, cell, logical) | ✅ |
| `while` / `end` | ✅ |
| `switch` / `case` / `otherwise` / `end` | ✅ |
| `case {1, 2, 3}` (cell matching in switch) | ✅ |
| `break`, `continue`, `return` | ✅ |
| `try` / `catch` / `end` | ✅ |
| `global` / `persistent` | ✅ |
| User-defined functions with `nargin`/`nargout` | ✅ |
| Multiple return values (`[a, b] = func(...)`) | ✅ |
| Recursive functions | ✅ |
| Anonymous functions with closures | ✅ |
| `end` keyword in indexing (`A(end)`, `A(end-1)`) | ✅ |
| Linear and subscript indexing | ✅ |
| Logical indexing | ✅ |
| Element deletion (`A(idx) = []`) | ✅ |
| 2D and 3D array support | ✅ |
| Command-style syntax (`clear all`, `grid on`) | ✅ |
| Implicit semicolon suppression | ✅ |
| Line continuation (`...`) | ✅ |
| Comments (`%`) and block comments (`%{ %}`) | ✅ |

### Built-in Functions

#### Math
`sqrt`, `abs`, `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `atan2`,
`exp`, `log`, `log2`, `log10`, `floor`, `ceil`, `round`, `fix`,
`mod`, `rem`, `sign`, `max`, `min`, `sum`, `prod`, `mean`, `cumsum`,
`linspace`, `logspace`, `rand`, `randn`, `deg2rad`, `rad2deg`

#### Matrix & Linear Algebra
`zeros`, `ones`, `eye`, `size`, `length`, `numel`, `ndims`, `reshape`,
`transpose`, `diag`, `sort`, `find`, `horzcat`, `vertcat`,
`cross`, `dot`, `norm`, `det`, `inv`, `eig`

#### Signal Processing
`fft`, `ifft`, `conv`

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

#### Higher-Order
`arrayfun`

#### Constants
`pi`, `eps`, `inf`, `Inf`, `nan`, `NaN`, `i`, `j`, `true`, `false`

### Plotting

MLab includes a figure management system that outputs plot data as JSON, rendered as SVG in the Web IDE.

#### Plot Types
`plot`, `bar`, `scatter`, `stem`, `stairs`, `hist`, `polarplot`,
`semilogx`, `semilogy`, `loglog`

#### Figure Management
`figure`, `close`, `clf`, `subplot`

#### Plot Configuration
`title`, `xlabel`, `ylabel`, `legend`, `xlim`, `ylim`,
`grid` (on/off/minor), `hold` (on/off), `axis` (equal/tight/ij/xy)

#### Polar Configuration
`rlim`, `thetalim`, `thetadir`, `thetazero`

#### Style
Style strings (`'r--o'`, `'b:'`, `'g-.'`) and name-value pairs (`'LineWidth'`, `'MarkerSize'`)

---

## Architecture

```
Source Code → Lexer → Tokens → Parser → AST → Engine (eval)
                                                  ↓
                                            FigureManager → JSON → Frontend (SVG)
```

| Module | File | Responsibility |
|---|---|---|
| **Lexer** | `MLabLexer.hpp/cpp` | Tokenization with implicit comma insertion inside `[]` |
| **Parser** | `MLabParser.hpp/cpp` | Recursive descent parser producing AST |
| **AST** | `MLabAst.hpp` | Node types, `unique_ptr`-based tree with `cloneNode()` |
| **Engine** | `MLabEngine.hpp/cpp` | Tree-walking interpreter with RAII guards |
| **Value** | `MLabValue.hpp/cpp` | Copy-on-write value system (double, complex, logical, char, cell, struct, function_handle) |
| **Environment** | `MLabEnvironment.hpp/cpp` | Scoped variable storage with global store |
| **Allocator** | `MLabAllocator.hpp/cpp` | Pluggable memory allocator |
| **StdLibrary** | `MLabStdLibrary.hpp/cpp` | All built-in operators and functions |
| **FigureManager** | `MLabFigureManager.hpp` | Plot state management with subplot/axes support |

### Key Design Decisions

- **Copy-on-Write (COW)** for matrix data — efficient passing without deep copies
- **Column-major storage** — matches MATLAB memory layout
- **`unique_ptr<ASTNode>`** — zero-overhead AST ownership, `shared_ptr` only for function bodies stored in the engine
- **RAII guards** — `IndexContextGuard` and `RecursionGuard` ensure exception safety
- **Pluggable allocator** — track memory, use custom pools, or integrate with your application's allocator
- **Non-copyable, non-movable `Engine`** — prevents dangling references in registered lambdas
- **Environment snapshots** — anonymous functions capture variables by value at creation time (MATLAB semantics)
- **AxesState / FigureState** — per-axes configuration supports subplot grids with independent settings
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

### Run

```bash
./mlab_example          # Run demo
./mlab_example --repl   # Interactive REPL
```

### Build Web IDE (WebAssembly)

```bash
# Requires Emscripten SDK
cd repl-vite
npm install
npm run build
```

### Run Tests

```bash
cd build
ctest --output-on-failure
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

### C++ ↔ MATLAB Data Exchange

```c++
mlab::Engine engine;
mlab::StdLibrary::install(engine);

// C++ → MATLAB
auto& alloc = engine.allocator();
engine.setVariable("radius", mlab::MValue::scalar(5.0, &alloc));
engine.eval("area = pi * radius^2;");

// MATLAB → C++
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
// FigureManager collects plot data as JSON
mlab::Engine engine;
mlab::StdLibrary::install(engine);

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

// Extract figure data as JSON for your renderer
auto& fm = engine.figureManager();
for (auto& [id, fig] : fm.figures()) {
    std::string json = fm.toJSON(id);
    // Send to your SVG/Canvas/WebGL renderer
}
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

- No sparse matrix support
- No object-oriented programming (classdef)
- No Simulink or toolbox functions
- Limited `fprintf`/`sprintf` formatting
- No file I/O (`fopen`, `fread`, etc.)
- No regular expressions (`regexp`)
- No GUI functions
- Matrix left division (`\`) only for scalars
- Matrix power (`^`) only for scalars
- No 3D plotting (`surf`, `mesh`, `plot3` — registered but no renderer)
- No `saveas` / figure export

---

## Project Structure

```
├── include/
│   ├── MLabAllocator.hpp
│   ├── MLabAst.hpp
│   ├── MLabEngine.hpp
│   ├── MLabEnvironment.hpp
│   ├── MLabFigureManager.hpp
│   ├── MLabLexer.hpp
│   ├── MLabParser.hpp
│   ├── MLabStdLibrary.hpp
│   └── MLabValue.hpp
├── src/
│   ├── MLabAllocator.cpp
│   ├── MLabEngine.cpp
│   ├── MLabEnvironment.cpp
│   ├── MLabLexer.cpp
│   ├── MLabParser.cpp
│   ├── MLabStdLibrary.cpp
│   └── MLabValue.cpp
├── tests/
│   ├── engine_test.cpp
│   ├── engine_advanced_test.cpp
│   ├── command_style_test.cpp
│   └── figure_test.cpp
├── example/
│   └── main.cpp
├── repl-vite/                  # Web IDE (React + Vite)
│   ├── src/
│   │   ├── components/
│   │   │   ├── MLabREPL.jsx    # Main IDE layout
│   │   │   ├── Console.jsx     # Command console
│   │   │   ├── Figures.jsx     # SVG plot renderer
│   │   │   ├── FileBrowser.jsx # Local/Examples/GitHub browser
│   │   │   ├── SyntaxEditor.jsx# MATLAB syntax highlighting
│   │   │   ├── Workspace.jsx   # Variable inspector
│   │   │   └── Reference.jsx   # Cheat sheet
│   │   ├── theme.jsx           # Dark/Light theme (React Context)
│   │   ├── engine.js           # WASM engine wrapper
│   │   └── vfs.js              # Virtual filesystem (IndexedDB)
│   └── public/
│       └── examples/           # 39 example .m scripts
├── CMakeLists.txt
└── README.md
```

## License

MIT License. See [LICENSE](LICENSE) for details.
