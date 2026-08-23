/// @file arithmetic.hpp
/// @ingroup group_image
// toolboxes/image/include/numkit/image/arithmetic/arithmetic.hpp
//
// Image arithmetic. Element-wise operations on image arrays with
// saturating semantics for integer types
// (uint8/uint16/int16) and pass-through for floating types.
//
// All operations broadcast scalars and accept mismatched-but-castable
// types (one input promotes to the other's class).

#pragma once

#include <memory_resource>
#include <numkit/value/value.hpp>

#include <vector>

namespace numkit::image {

/// @addtogroup group_image
/// @{


/// Saturating image addition (`Z = imadd(X, Y)`).
///
/// Element-wise @f$ Z = X + Y @f$ with saturation for
/// integer outputs (uint8 → clamp to [0, 255], uint16 → [0, 65535],
/// int16 → [-32768, 32767]). Floating-point inputs pass through
/// without clipping.
///
/// Either operand may be a scalar (broadcasts). Output class equals
/// `X`'s class; `Y` is cast on the fly if needed.
///
/// @param x   First image (or scalar).
/// @param y   Second image (or scalar).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Image of the same class and shape as `X`.
///
/// @see imsubtract, immultiply, imdivide
Value imadd(const Value &x, const Value &y,
            std::pmr::memory_resource *mr = nullptr);

/// @brief Saturating image subtraction (`Z = imsubtract(X, Y)`).
///
/// Same semantics as @ref imadd but for `Z = X - Y`.
///
/// @param x   First image (or scalar).
/// @param y   Second image (or scalar).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Image of the same class and shape as `X`.
/// @see imadd, imabsdiff
Value imsubtract(const Value &x, const Value &y,
                 std::pmr::memory_resource *mr = nullptr);

/// @brief Saturating element-wise multiplication
/// (`Z = immultiply(X, Y)`).
///
/// @param x   First image (or scalar).
/// @param y   Second image (or scalar).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Image of the same class and shape as `X`.
/// @see imadd, imdivide
Value immultiply(const Value &x, const Value &y,
                 std::pmr::memory_resource *mr = nullptr);

/// @brief Saturating element-wise division
/// (`Z = imdivide(X, Y)`).
///
/// Division by zero in integer classes saturates to the output
/// type's max value; in floats it produces `Inf`/`NaN` per IEEE-754.
///
/// @param x   Numerator image (or scalar).
/// @param y   Denominator image (or scalar).
/// @param mr  Memory resource (nullptr → process default).
/// @return    Image of the same class and shape as `X`.
/// @see imadd, immultiply
Value imdivide(const Value &x, const Value &y,
               std::pmr::memory_resource *mr = nullptr);

/// @brief Saturating absolute difference (`Z = imabsdiff(X, Y)`).
///
/// Computes `Z = |X − Y|` with saturation.
///
/// @param x   First image.
/// @param y   Second image.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Image of the same class and shape as `X`.
/// @see imsubtract
Value imabsdiff(const Value &x, const Value &y,
                std::pmr::memory_resource *mr = nullptr);

/// Image complement (`Y = imcomplement(X)`).
///
/// - integer classes: `Y = MAX(class) - X` (uint8 → 255 − X, etc.).
/// - floating point: `Y = 1 - X` (assumes input in [0, 1]).
/// - logical: `Y = ~X` (bitwise complement).
///
/// @param x   Input image.
/// @param mr  Memory resource (nullptr → process default).
/// @return    Complemented image of the same class.
Value imcomplement(const Value &x, std::pmr::memory_resource *mr = nullptr);

/// Linear combination of images (`Z = imlincomb(coefs, images, class)`).
///
/// Computes @f$ Z = \sum_k c_k\,X_k @f$ and casts the result to
/// `output_class` (with saturation for integer classes).
/// All images must share the same size.
///
/// @param coefs         1-D vector of weights @f$ c_k @f$.
/// @param images        Vector of input images, same length as `coefs`.
/// @param output_class  Target class for the result.
/// @param mr            Memory resource (nullptr → process default).
/// @return              Linear combination cast to `output_class`.
/// @throws              Error if sizes disagree or the vectors are empty.
Value imlincomb(const std::vector<double> &coefs,
                const std::vector<Value> &images,
                ValueType output_class,
                std::pmr::memory_resource *mr = nullptr);

/// Apply a colour-space transform matrix (`Y = imapplymatrix(M, X)`).
///
/// For an H×W×N image `X` and an M×N matrix `M`, computes the per-pixel
/// matrix-vector product along the third axis: `Y(:,:,i) = sum_j M(i,j) · X(:,:,j)`.
/// Typical use is a 3×3 RGB-to-YCbCr / sRGB-to-XYZ transform.
///
/// @param M             M×N transform matrix.
/// @param x             H×W×N input image.
/// @param output_class  Target class (defaults to X's float type).
/// @param mr            Memory resource (nullptr → process default).
/// @return              H×W×M transformed image.
/// @throws              Error if M cols ≠ X pages.
Value imapplymatrix(const Value &M, const Value &x,
                    ValueType output_class,
                    std::pmr::memory_resource *mr = nullptr);


/// @}
} // namespace numkit::image
