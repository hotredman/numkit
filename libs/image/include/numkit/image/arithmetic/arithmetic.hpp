// libs/image/include/numkit/image/arithmetic/arithmetic.hpp
//
// Image arithmetic. Element-wise operations on image arrays with
// MATLAB-style saturation semantics for integer types
// (uint8/uint16/int16) and pass-through for floating types.
//
// All operations broadcast scalars and accept mismatched-but-castable
// types (one input promotes to the other's class).

#pragma once

#include <memory_resource>
#include <numkit/core/value.hpp>

#include <vector>

namespace numkit::image {

/// imadd(X, Y) — saturating element-wise X + Y. Output class = X's class.
Value imadd(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr);

/// imsubtract(X, Y) — saturating X - Y. Output class = X's class.
Value imsubtract(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr);

/// immultiply(X, Y) — saturating X .* Y. Output class = X's class.
Value immultiply(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr);

/// imdivide(X, Y) — saturating X ./ Y. Output class = X's class.
Value imdivide(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr);

/// imabsdiff(X, Y) — saturating |X - Y|. Output class = X's class.
Value imabsdiff(const Value &x, const Value &y, std::pmr::memory_resource *mr = nullptr);

/// imcomplement(X) — image complement. For integer classes:
/// MAX(class) - X. For floating-point input in [0, 1]: 1 - X.
Value imcomplement(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// imlincomb(coefs, images, [output_class]) — Σ_k cₖ·Xₖ with optional
/// final cast to output_class. coefs is a 1-D vector of doubles;
/// images is a cell array of same-size matrices.
Value imlincomb(const std::vector<double> &coefs, const std::vector<Value> &images, ValueType output_class, std::pmr::memory_resource *mr = nullptr);

/// imapplymatrix(M, X[, output_class]) — apply 3×3 (or N×N) colour
/// transform along the third axis of X. Output class defaults to X's
/// floating type.
Value imapplymatrix(const Value &M, const Value &x, ValueType output_class, std::pmr::memory_resource *mr = nullptr);

} // namespace numkit::image
