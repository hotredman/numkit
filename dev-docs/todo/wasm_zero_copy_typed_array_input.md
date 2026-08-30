# todo: Zero-copy TypedArray variable injection into WASM engine

*Kind:* feature / perf · *Status:* open · *Surfaced:* 2026-08-30

> Lifecycle: open -> done. On completion, record the outcome in
> `dev-docs/memory/` (per the AGENTS.md project-memory protocol) and
> delete this file — the todo list holds open work only.

## Goal
Add high-performance direct data injection methods (`repl_set_var_float64`, `repl_set_var_typed_array`) to the Emscripten WASM bridge, allowing JavaScript/TypeScript applications and AI agent workers to transfer large numeric buffers (e.g., 1M+ sample audio streams, sensor logs, images, and large matrices) directly from JS `Float64Array` / `TypedArray` into NumKit workspace variables without string serialization or parser overhead.

## Problem & Rationale
1. **String Parsing Overhead**: Setting variables by constructing MATLAB string code (`mod.repl_execute("x = [1.23, 4.56, ...]")`) requires text concatenation in JS and lexer/parser/AST allocation in C++. For arrays with $> 100{,}000$ elements, this adds noticeable latency and garbage collector pressure.
2. **Zero-Copy / Fast Memcpy Efficiency**: Emscripten allows direct memory transfer between JS `Float64Array` views and C++ memory buffers (`std::vector<double>` or `numkit::TensorStorage`) via raw pointer `memcpy`, completing in $< 0.5\text{ ms}$ for megabyte-scale datasets.

## Proposed Design & Implementation

### 1. C++ Emscripten Binding (`wasm/src/repl_bindings.cpp`)
```cpp
void repl_set_var_float64(const std::string &name,
                          emscripten::val typedArray,
                          size_t rows,
                          size_t cols) {
    if (!g_session) return;
    
    // Extract TypedArray length and raw elements
    const size_t numel = rows * cols;
    std::vector<double> buf(numel);
    emscripten::val memoryView = emscripten::val::global("Float64Array").new_(
        emscripten::val::module_property("HEAPF64")["buffer"],
        reinterpret_cast<uintptr_t>(buf.data()),
        numel
    );
    memoryView.call<void>("set", typedArray);
    
    // Construct numkit::Value and assign to active workspace
    numkit::Value val = numkit::Value::fromMatrix(rows, cols, std::move(buf));
    g_session->setVariable(name, val);
}
```

### 2. Supported Types in Follow-ups
- `repl_set_var_float64` — 64-bit double matrices (default MATLAB numeric type).
- `repl_set_var_int32` / `repl_set_var_uint8` — integer matrices (images and digital signals).
- `repl_set_var_complex64` — interleaved real/imaginary complex float64 arrays.

## Acceptance Criteria
- [ ] `repl_set_var_float64` exported in `EMSCRIPTEN_BINDINGS` (`wasm/src/repl_bindings.cpp`).
- [ ] A 1,000,000-element `Float64Array` injected from JS into variable `x` in $< 1\text{ ms}$.
- [ ] Immediate execution of `y = fft(x);` or `m = mean(x);` in the same session without workspace errors.
- [ ] Regression test added to `packages/numkit/test/` verifying shape, values, and memory safety.
