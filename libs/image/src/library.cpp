// libs/image/src/library.cpp
//
// Registration hub for the Image Processing Toolbox builtins.
// Namespace: image.<sub>.<name>; every function is also aliased into
// `compat.<name>` so MATLAB-style scripts can call them flat.

#include <numkit/image/library.hpp>

#include <numkit/core/types.hpp>

namespace numkit::image::detail {
// arithmetic/arithmetic.cpp
void imadd_reg          (Span<const Value>, size_t, Span<Value>, CallContext &);
void imsubtract_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void immultiply_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void imdivide_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);
void imabsdiff_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void imcomplement_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void imlincomb_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void imapplymatrix_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);

// type_convert/type_convert.cpp
void im2double_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void im2single_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void im2uint8_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void im2uint16_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void im2int16_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void mat2gray_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void im2gray_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void rgb2gray_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
} // namespace numkit::image::detail

namespace numkit {

void ImageLibrary::install(Engine &engine)
{
    auto reg = [&](const char *sub, const char *name, ExternalFunc fn) {
        engine.registerFunction(std::string("image.") + sub, name, fn);
        engine.registerFunction("compat", name, fn);
    };

    reg("arith", "imadd",         &image::detail::imadd_reg);
    reg("arith", "imsubtract",    &image::detail::imsubtract_reg);
    reg("arith", "immultiply",    &image::detail::immultiply_reg);
    reg("arith", "imdivide",      &image::detail::imdivide_reg);
    reg("arith", "imabsdiff",     &image::detail::imabsdiff_reg);
    reg("arith", "imcomplement",  &image::detail::imcomplement_reg);
    reg("arith", "imlincomb",     &image::detail::imlincomb_reg);
    reg("arith", "imapplymatrix", &image::detail::imapplymatrix_reg);

    reg("type",  "im2double",     &image::detail::im2double_reg);
    reg("type",  "im2single",     &image::detail::im2single_reg);
    reg("type",  "im2uint8",      &image::detail::im2uint8_reg);
    reg("type",  "im2uint16",     &image::detail::im2uint16_reg);
    reg("type",  "im2int16",      &image::detail::im2int16_reg);
    reg("type",  "mat2gray",      &image::detail::mat2gray_reg);
    reg("type",  "im2gray",       &image::detail::im2gray_reg);
    reg("color", "rgb2gray",      &image::detail::rgb2gray_reg);
}

} // namespace numkit
