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
}

} // namespace numkit
