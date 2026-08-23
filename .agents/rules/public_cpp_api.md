# Public C++ API Standards

All public C++ headers in `include/numkit/` (particularly `src/builtin/include/numkit/builtin/*.hpp` and `src/toolboxes/*/include/`) must strictly adhere to the following architectural and documentation standards.

## 1. Doxygen Documentation Requirements
Every public C++ function must feature a complete, multi-part Doxygen comment:
- **`@brief`**: Clear summary of the operation, including algebraic formula where applicable (e.g. `Elementwise addition (y = a + b)`).
- **Detailed Description**:
  - Broadcasting & shape behavior (e.g. singleton expansion rules).
  - Type promotion, integer saturation, or complex number generation.
  - Dimensionality constraints (e.g., inner dimension matching for matrix multiplication).
- **`@param`**: Document every parameter explicitly:
  - Input operands: description, role, dimensionality, scalar handling.
  - Memory resource: `@param mr Memory resource for allocations (nullptr for default).`
- **`@return`**: Explicit description of return value and output array shape.
- **`@throws`**: Document expected exceptions on invalid inputs or dimension mismatches.
- **`@see`**: Cross-references to inverse, complementary, and related elementwise/matrix functions (e.g. `@see rdivide, ldivide, mrdivide`).

## 2. Complete Engine Decoupling & Layer Separation
- **L2 Compute Headers** (`src/builtin/include/numkit/builtin/*.hpp` and `src/toolboxes/*/include/...`) **MUST be 100% engine-free**:
  - They must NOT include `<numkit/core/engine.hpp>` or `<numkit/core/types.hpp>`.
  - They must NOT accept `Engine&` or `CallContext&`, and must NOT declare `register_*` functions.
  - L2 headers and sources depend exclusively on L0 (`value`), L0.5 (`ops`), and the standard library (`<memory_resource>`, `<complex>`, `<cmath>`, etc.).
- **L3 Registration Layer** (`src/bundle/src/register/` and `src/bundle/include/`):
  - All engine registration functions (`register_<category>(Engine&)`, `*_reg(...)` wrappers, `BuiltinLibrary::install(Engine&)`) belong exclusively to Layer L3 (`src/bundle/`).

## 3. Polymorphism & PMR Allocation
- Every function returning a `Value` or allocating dynamic storage must accept `std::pmr::memory_resource *mr = nullptr` as its final parameter.

## 4. C++ Keyword Collision Handling
- For MATLAB names that collide with C++ keywords (`and`, `or`, `not`, `xor`), provide descriptive names (`logical_and`, `logical_or`, `logical_not`, `logical_xor`) and concise aliases (`and_op`, `or_op`, `not_op`, `xor_op`).
