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

// color/color.cpp
void rgb2hsv_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void hsv2rgb_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void rgb2ycbcr_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void ycbcr2rgb_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void rgb2xyz_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void xyz2rgb_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void rgb2lab_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void lab2rgb_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void xyz2lab_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void lab2xyz_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);

// filter/filter.cpp
void padarray_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void fspecial_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void imfilter_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void imgaussfilt_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void imboxfilt_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void medfilt2_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);

// contrast/contrast.cpp
void imhist_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void stretchlim_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void imadjust_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void histeq_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void graythresh_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void otsuthresh_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void multithresh_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void imbinarize_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void imquantize_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);

// morph/morph.cpp
void strel_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);
void imerode_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void imdilate_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void imopen_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void imclose_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);

// region/region.cpp
void bwlabel_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void bwconncomp_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void bwarea_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void bwperim_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void bwareaopen_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);

// object/object.cpp
void imgradientxy_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void imgradient_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void edge_reg        (Span<const Value>, size_t, Span<Value>, CallContext &);

// quality/quality.cpp
void immse_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);
void psnr_reg        (Span<const Value>, size_t, Span<Value>, CallContext &);
void ssim_reg        (Span<const Value>, size_t, Span<Value>, CallContext &);

// transform/transform.cpp
void dct2_reg        (Span<const Value>, size_t, Span<Value>, CallContext &);
void idct2_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);
void dctmtx_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
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

    reg("color", "rgb2hsv",       &image::detail::rgb2hsv_reg);
    reg("color", "hsv2rgb",       &image::detail::hsv2rgb_reg);
    reg("color", "rgb2ycbcr",     &image::detail::rgb2ycbcr_reg);
    reg("color", "ycbcr2rgb",     &image::detail::ycbcr2rgb_reg);
    reg("color", "rgb2xyz",       &image::detail::rgb2xyz_reg);
    reg("color", "xyz2rgb",       &image::detail::xyz2rgb_reg);
    reg("color", "rgb2lab",       &image::detail::rgb2lab_reg);
    reg("color", "lab2rgb",       &image::detail::lab2rgb_reg);
    reg("color", "xyz2lab",       &image::detail::xyz2lab_reg);
    reg("color", "lab2xyz",       &image::detail::lab2xyz_reg);

    reg("filter", "padarray",     &image::detail::padarray_reg);
    reg("filter", "fspecial",     &image::detail::fspecial_reg);
    reg("filter", "imfilter",     &image::detail::imfilter_reg);
    reg("filter", "imgaussfilt",  &image::detail::imgaussfilt_reg);
    reg("filter", "imboxfilt",    &image::detail::imboxfilt_reg);
    reg("filter", "medfilt2",     &image::detail::medfilt2_reg);

    reg("contrast", "imhist",     &image::detail::imhist_reg);
    reg("contrast", "stretchlim", &image::detail::stretchlim_reg);
    reg("contrast", "imadjust",   &image::detail::imadjust_reg);
    reg("contrast", "histeq",     &image::detail::histeq_reg);

    reg("type",  "graythresh",   &image::detail::graythresh_reg);
    reg("type",  "otsuthresh",   &image::detail::otsuthresh_reg);
    reg("type",  "multithresh",  &image::detail::multithresh_reg);
    reg("type",  "imbinarize",   &image::detail::imbinarize_reg);
    reg("type",  "imquantize",   &image::detail::imquantize_reg);

    reg("morph", "strel",     &image::detail::strel_reg);
    reg("morph", "imerode",   &image::detail::imerode_reg);
    reg("morph", "imdilate",  &image::detail::imdilate_reg);
    reg("morph", "imopen",    &image::detail::imopen_reg);
    reg("morph", "imclose",   &image::detail::imclose_reg);

    reg("region", "bwlabel",    &image::detail::bwlabel_reg);
    reg("region", "bwconncomp", &image::detail::bwconncomp_reg);
    reg("region", "bwarea",     &image::detail::bwarea_reg);
    reg("region", "bwperim",    &image::detail::bwperim_reg);
    reg("region", "bwareaopen", &image::detail::bwareaopen_reg);

    reg("object", "imgradientxy", &image::detail::imgradientxy_reg);
    reg("object", "imgradient",   &image::detail::imgradient_reg);
    reg("object", "edge",         &image::detail::edge_reg);

    reg("quality", "immse", &image::detail::immse_reg);
    reg("quality", "psnr",  &image::detail::psnr_reg);
    reg("quality", "ssim",  &image::detail::ssim_reg);

    reg("transform", "dct2",   &image::detail::dct2_reg);
    reg("transform", "idct2",  &image::detail::idct2_reg);
    reg("transform", "dctmtx", &image::detail::dctmtx_reg);
}

} // namespace numkit
