# MLab — Embeddable MATLAB Interpreter in C++ by Claude Opus 4.6

A lightweight, embeddable interpreter for a substantial subset of the MATLAB language, written in modern C++17. Designed for embedding MATLAB-like scripting capabilities into C++ applications with full control over memory allocation, I/O, and extensibility.

[![Try Online](https://img.shields.io/badge/Try%20Online-MLab%20REPL-blue?style=for-the-badge&logo=webassembly)](https://picosaur.github.io/MLab/)

**[▶ Launch MLab REPL in Browser](https://picosaur.github.io/MLab/)**

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
| Logical operators (`&`, `|`, `~`, `&&`, `||`) | ✅ |
| Short-circuit evaluation (`&&`, `||`) | ✅ |
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
| Implicit semicolon suppression | ✅ |
| Line continuation (`...`) | ✅ |
| Comments (`%`) | ✅ |

### Built-in Functions

#### Math
`sqrt`, `abs`, `sin`, `cos`, `tan`, `exp`, `log`, `log2`, `log10`,
`floor`, `ceil`, `round`, `fix`, `mod`, `rem`, `sign`, `max`, `min`,
`sum`, `prod`, `mean`, `linspace`, `rand`, `randn`

#### Matrix
`zeros`, `ones`, `eye`, `size`, `length`, `numel`, `ndims`, `reshape`,
`transpose`, `diag`, `sort`, `find`, `horzcat`, `vertcat`

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
`clear`, `who`, `whos`, `exist`, `class`

#### Constants
`pi`, `eps`, `inf`, `Inf`, `nan`, `NaN`, `i`, `j`, `true`, `false`

## Architecture
Source Code → Lexer → Tokens → Parser → AST → Engine (eval)


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

### Key Design Decisions

- **Copy-on-Write (COW)** for matrix data — efficient passing without deep copies
- **Column-major storage** — matches MATLAB memory layout
- **`unique_ptr<ASTNode>`** — zero-overhead AST ownership, `shared_ptr` only for function bodies stored in the engine
- **RAII guards** — `IndexContextGuard` and `RecursionGuard` ensure exception safety
- **Pluggable allocator** — track memory, use custom pools, or integrate with your application's allocator
- **Non-copyable, non-movable `Engine`** — prevents dangling references in registered lambdas
- **Environment snapshots** — anonymous functions capture variables by value at creation time (MATLAB semantics)

## Building

### Requirements

- C++17 compiler (GCC 8+, Clang 7+, MSVC 2019+)
- CMake 3.14+

### Build

```bash
mkdir build && cd build
cmake ..
cmake --build .

### Run
./mlab_example          # Run demo
./mlab_example --repl   # Interactive REPL
```

### Run
```Shell
./mlab_example          # Run demo
./mlab_example --repl   # Interactive REPL
```
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
### Custom Output
```c++
mlab::Engine engine;
mlab::StdLibrary::install(engine);

std::string captured;
engine.setOutputFunc([&captured](const std::string& s) {
    captured += s;
});

engine.eval("disp('Hello from MATLAB')");
// captured == "Hello from MATLAB\n"
```
### Registering Custom Functions
```c++
mlab::Engine engine;
mlab::StdLibrary::install(engine);

engine.registerFunction("myfunc",
    [&engine](const std::vector<mlab::MValue>& args) -> std::vector<mlab::MValue> {
        auto* alloc = &engine.allocator();
        double x = args[0].toScalar();
        double y = args[1].toScalar();
        return {mlab::MValue::scalar(x * x + y * y, alloc)};
    });

engine.eval("disp(myfunc(3, 4))");  // 25
```
### Complex Numbers
```octave
z = 3 + 4i;
disp(abs(z))      % 5
disp(real(z))     % 3
disp(imag(z))     % 4
disp(conj(z))     % 3 - 4i
disp(z * (1+1i))  % -1 + 7i
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
### Cell Arrays
```octave
c = {1, 'hello'; [1 2 3], true};
disp(c{1, 2})    % hello
disp(c{2, 1})    % [1 2 3]

% Cell matching in switch
switch day
    case {1, 7}
        disp('weekend')
    case {2, 3, 4, 5, 6}
        disp('weekday')
end
```
### Error Handling
MATLAB

```octave
try
    x = 1 / 0;
    error('something went wrong')
catch e
    disp(e.message)
end
```
## Limitations
- No sparse matrix support
- No object-oriented programming (classdef)
- No Simulink or toolbox functions
- Limited `fprintf`/`sprintf` formatting (no format specifiers yet)
- No file I/O (`fopen`, `fread`, etc.)
- No regular expressions (`regexp`)
- No GUI functions
- Matrix left division (`\`) only for scalars
- Matrix power (`^`) only for scalars
- Integer types declared in `MType` but not fully implemented
## Project Structure
```
├── include/
│   ├── MLabAllocator.hpp
│   ├── MLabAst.hpp
│   ├── MLabEngine.hpp
│   ├── MLabEnvironment.hpp
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
├── example/
│   └── main.cpp
├── CMakeLists.txt
└── README.md
```
## License
MIT License. See [LICENSE](https://arena.ai/c/LICENSE) for details.
