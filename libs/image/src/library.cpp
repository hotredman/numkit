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
void imsplit_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void intlut_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void isbw_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void isgray_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void isind_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void isrgb_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void getrangefromclass_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void imcast_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void iptnum2ordinal_reg(Span<const Value>, size_t, Span<Value>, CallContext &);

// color/color.cpp
void rgb2hsv_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void hsv2rgb_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void rgb2ycbcr_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void ycbcr2rgb_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void rgb2ntsc_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void ntsc2rgb_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void rgb2xyz_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void xyz2rgb_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void rgb2lab_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void lab2rgb_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void xyz2lab_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void lab2xyz_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void label2rgb_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void colorangle_reg(Span<const Value>, size_t, Span<Value>, CallContext &);

// filter/filter.cpp
void padarray_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void fspecial_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void imfilter_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void imgaussfilt_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void imboxfilt_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void medfilt2_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void imsharpen_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void imnoise_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void stdfilt_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void rangefilt_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void entropyfilt_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void wiener2_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void ordfilt2_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void im2col_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void col2im_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void imbilatfilt_reg (Span<const Value>, size_t, Span<Value>, CallContext &);

// contrast/contrast.cpp
void imhist_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void stretchlim_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void imadjust_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void histeq_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void adaptthresh_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void imhistmatch_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void imflatfield_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void grayslice_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void entropy_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
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
void imreconstruct_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void imfill_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void imregionalmax_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void imregionalmin_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void imhmax_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);
void imhmin_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);
void imextendedmax_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void imextendedmin_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void imimposemin_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void imclearborder_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void imkeepborder_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void imtophat_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void imbothat_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void bwhitmiss_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void applylut_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void mmgradm_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void bwpack_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);
void bwunpack_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);

// region/region.cpp
void bwlabel_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void bwconncomp_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void bwarea_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void bwperim_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void bwareaopen_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void bwboundaries_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void regionprops_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void bwdist_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void bweuler_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void bwareafilt_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void fchcode_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);

// object/object.cpp
void imgradientxy_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void imgradient_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void edge_reg        (Span<const Value>, size_t, Span<Value>, CallContext &);

// quality/quality.cpp
void immse_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);
void psnr_reg        (Span<const Value>, size_t, Span<Value>, CallContext &);
void ssim_reg        (Span<const Value>, size_t, Span<Value>, CallContext &);
void mean2_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);
void std2_reg        (Span<const Value>, size_t, Span<Value>, CallContext &);
void corr2_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);

// transform/transform.cpp
void dct2_reg          (Span<const Value>, size_t, Span<Value>, CallContext &);
void idct2_reg         (Span<const Value>, size_t, Span<Value>, CallContext &);
void dctmtx_reg        (Span<const Value>, size_t, Span<Value>, CallContext &);
void integralImage_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void integralImage3_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void checkerboard_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void normxcorr2_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void phantom_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);
void psf2otf_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);
void otf2psf_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);
void fftconv2_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void bestblk_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);

// io/io.cpp
void imread_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void imwrite_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void imfinfo_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);

// segment/segment.cpp
void dice_reg           (Span<const Value>, size_t, Span<Value>, CallContext &);
void jaccard_reg        (Span<const Value>, size_t, Span<Value>, CallContext &);
void boundarymask_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void label2idx_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void grayconnected_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void imoverlay_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);

// geom/geom.cpp
void imresize_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void imcrop_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void imrotate_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void imtranslate_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void impyramid_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void axes2pix_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
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
    reg("type",  "intlut",        &image::detail::intlut_reg);
    reg("type",  "isbw",          &image::detail::isbw_reg);
    reg("type",  "isgray",        &image::detail::isgray_reg);
    reg("type",  "isind",         &image::detail::isind_reg);
    reg("type",  "isrgb",         &image::detail::isrgb_reg);
    reg("type",  "getrangefromclass",
                                  &image::detail::getrangefromclass_reg);
    reg("type",  "imcast",        &image::detail::imcast_reg);
    reg("type",  "iptnum2ordinal",
                                  &image::detail::iptnum2ordinal_reg);
    reg("color", "rgb2gray",      &image::detail::rgb2gray_reg);
    reg("color", "imsplit",       &image::detail::imsplit_reg);

    reg("color", "rgb2hsv",       &image::detail::rgb2hsv_reg);
    reg("color", "hsv2rgb",       &image::detail::hsv2rgb_reg);
    reg("color", "rgb2ycbcr",     &image::detail::rgb2ycbcr_reg);
    reg("color", "ycbcr2rgb",     &image::detail::ycbcr2rgb_reg);
    reg("color", "rgb2ntsc",      &image::detail::rgb2ntsc_reg);
    reg("color", "ntsc2rgb",      &image::detail::ntsc2rgb_reg);
    reg("color", "rgb2xyz",       &image::detail::rgb2xyz_reg);
    reg("color", "xyz2rgb",       &image::detail::xyz2rgb_reg);
    reg("color", "rgb2lab",       &image::detail::rgb2lab_reg);
    reg("color", "lab2rgb",       &image::detail::lab2rgb_reg);
    reg("color", "xyz2lab",       &image::detail::xyz2lab_reg);
    reg("color", "lab2xyz",       &image::detail::lab2xyz_reg);
    reg("color", "label2rgb",     &image::detail::label2rgb_reg);
    reg("color", "colorangle",    &image::detail::colorangle_reg);

    reg("filter", "padarray",     &image::detail::padarray_reg);
    reg("filter", "fspecial",     &image::detail::fspecial_reg);
    reg("filter", "imfilter",     &image::detail::imfilter_reg);
    reg("filter", "imgaussfilt",  &image::detail::imgaussfilt_reg);
    reg("filter", "imboxfilt",    &image::detail::imboxfilt_reg);
    reg("filter", "medfilt2",     &image::detail::medfilt2_reg);
    reg("filter", "imsharpen",    &image::detail::imsharpen_reg);
    reg("filter", "imnoise",      &image::detail::imnoise_reg);
    reg("filter", "stdfilt",      &image::detail::stdfilt_reg);
    reg("filter", "rangefilt",    &image::detail::rangefilt_reg);
    reg("filter", "entropyfilt",  &image::detail::entropyfilt_reg);
    reg("filter", "wiener2",      &image::detail::wiener2_reg);
    reg("filter", "ordfilt2",     &image::detail::ordfilt2_reg);
    reg("filter", "im2col",       &image::detail::im2col_reg);
    reg("filter", "col2im",       &image::detail::col2im_reg);
    reg("filter", "imbilatfilt",  &image::detail::imbilatfilt_reg);

    // im2bw is the legacy alias of imbinarize (signatures match exactly:
    // im2bw(I) → Otsu, im2bw(I, level) → scalar threshold).
    reg("type",   "im2bw",        &image::detail::imbinarize_reg);

    reg("contrast", "imhist",     &image::detail::imhist_reg);
    reg("contrast", "stretchlim", &image::detail::stretchlim_reg);
    reg("contrast", "imadjust",   &image::detail::imadjust_reg);
    reg("contrast", "histeq",     &image::detail::histeq_reg);
    reg("contrast", "adaptthresh",&image::detail::adaptthresh_reg);
    reg("contrast", "imhistmatch",&image::detail::imhistmatch_reg);
    reg("contrast", "imflatfield",&image::detail::imflatfield_reg);
    reg("contrast", "grayslice",  &image::detail::grayslice_reg);
    reg("contrast", "entropy",    &image::detail::entropy_reg);

    // imadjustn — N-D variant of imadjust. Our imadjust already
    // handles 3-D arrays elementwise on the unit-range mapping, so
    // an alias is exact: imadjustn(I) ≡ imadjust(I).
    reg("contrast", "imadjustn",  &image::detail::imadjust_reg);

    // imhistmatchn — N-D variant of imhistmatch. Our imhistmatch
    // builds a single histogram across all elements (as imhistmatchn
    // does on volumes); aliasing covers both names with the same
    // single-histogram semantics.
    reg("contrast", "imhistmatchn",&image::detail::imhistmatch_reg);

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
    reg("morph", "imreconstruct", &image::detail::imreconstruct_reg);
    reg("morph", "imfill",        &image::detail::imfill_reg);
    reg("morph", "imregionalmax", &image::detail::imregionalmax_reg);
    reg("morph", "imregionalmin", &image::detail::imregionalmin_reg);
    reg("morph", "imhmax",        &image::detail::imhmax_reg);
    reg("morph", "imhmin",        &image::detail::imhmin_reg);
    reg("morph", "imextendedmax", &image::detail::imextendedmax_reg);
    reg("morph", "imextendedmin", &image::detail::imextendedmin_reg);
    reg("morph", "imimposemin",   &image::detail::imimposemin_reg);
    reg("morph", "imclearborder", &image::detail::imclearborder_reg);
    reg("morph", "imkeepborder",  &image::detail::imkeepborder_reg);
    reg("morph", "imtophat",      &image::detail::imtophat_reg);
    reg("morph", "imbothat",      &image::detail::imbothat_reg);
    reg("morph", "bwhitmiss",     &image::detail::bwhitmiss_reg);
    reg("morph", "applylut",      &image::detail::applylut_reg);
    reg("morph", "mmgradm",       &image::detail::mmgradm_reg);
    reg("morph", "bwpack",        &image::detail::bwpack_reg);
    reg("morph", "bwunpack",      &image::detail::bwunpack_reg);

    reg("region", "bwlabel",      &image::detail::bwlabel_reg);
    reg("region", "bwconncomp",   &image::detail::bwconncomp_reg);
    reg("region", "bwarea",       &image::detail::bwarea_reg);
    reg("region", "bwperim",      &image::detail::bwperim_reg);
    reg("region", "bwareaopen",   &image::detail::bwareaopen_reg);
    reg("region", "bwboundaries", &image::detail::bwboundaries_reg);
    reg("region", "regionprops",  &image::detail::regionprops_reg);
    reg("region", "bwdist",       &image::detail::bwdist_reg);
    reg("region", "bweuler",      &image::detail::bweuler_reg);
    reg("region", "bwareafilt",   &image::detail::bwareafilt_reg);
    reg("region", "fchcode",      &image::detail::fchcode_reg);

    reg("object", "imgradientxy", &image::detail::imgradientxy_reg);
    reg("object", "imgradient",   &image::detail::imgradient_reg);
    reg("object", "edge",         &image::detail::edge_reg);

    reg("quality", "immse", &image::detail::immse_reg);
    reg("quality", "psnr",  &image::detail::psnr_reg);
    reg("quality", "ssim",  &image::detail::ssim_reg);
    reg("quality", "mean2", &image::detail::mean2_reg);
    reg("quality", "std2",  &image::detail::std2_reg);
    reg("quality", "corr2", &image::detail::corr2_reg);

    reg("transform", "dct2",          &image::detail::dct2_reg);
    reg("transform", "idct2",         &image::detail::idct2_reg);
    reg("transform", "dctmtx",        &image::detail::dctmtx_reg);
    reg("transform", "integralImage", &image::detail::integralImage_reg);
    reg("transform", "integralImage3",&image::detail::integralImage3_reg);
    reg("transform", "checkerboard",  &image::detail::checkerboard_reg);
    reg("transform", "normxcorr2",    &image::detail::normxcorr2_reg);
    reg("transform", "psf2otf",       &image::detail::psf2otf_reg);
    reg("transform", "otf2psf",       &image::detail::otf2psf_reg);
    reg("transform", "fftconv2",      &image::detail::fftconv2_reg);
    reg("transform", "bestblk",       &image::detail::bestblk_reg);
    reg("transform", "phantom",       &image::detail::phantom_reg);

    reg("io", "imread",  &image::detail::imread_reg);
    reg("io", "imwrite", &image::detail::imwrite_reg);
    reg("io", "imfinfo", &image::detail::imfinfo_reg);

    reg("geom", "imresize",    &image::detail::imresize_reg);
    reg("geom", "imcrop",      &image::detail::imcrop_reg);
    reg("geom", "imrotate",    &image::detail::imrotate_reg);
    reg("geom", "imtranslate", &image::detail::imtranslate_reg);
    reg("geom", "impyramid",   &image::detail::impyramid_reg);
    reg("geom", "axes2pix",    &image::detail::axes2pix_reg);

    reg("segment", "dice",          &image::detail::dice_reg);
    reg("segment", "jaccard",       &image::detail::jaccard_reg);
    reg("segment", "boundarymask",  &image::detail::boundarymask_reg);
    reg("segment", "label2idx",     &image::detail::label2idx_reg);
    reg("segment", "grayconnected", &image::detail::grayconnected_reg);
    reg("segment", "imoverlay",     &image::detail::imoverlay_reg);
}

} // namespace numkit
