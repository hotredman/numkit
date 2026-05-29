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
void iscolormap_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void getrangefromclass_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void imcast_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void iptnum2ordinal_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void gray2ind_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void ind2gray_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void ind2rgb_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);

// color/color.cpp
void rgb2hsv_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void hsv2rgb_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void rgb2ycbcr_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void ycbcr2rgb_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void rgb2ntsc_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void ntsc2rgb_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void lab2double_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void lab2single_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void lab2uint8_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void lab2uint16_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void rgb2xyz_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void xyz2rgb_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void rgb2lab_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
// color/color_extras.cpp
void rgb2lightness_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void rgb2ind_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void lab2rgb_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void xyz2lab_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void lab2xyz_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void label2rgb_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void colorangle_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void wavelength2rgb_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void colorgradient_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void cmap2gray_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void gray_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void hot_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);
void cool_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void spring_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void summer_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void autumn_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void winter_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void copper_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void pink_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void hsv_cmap_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void flag_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void prism_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void lines_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void bone_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void white_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void rgb2lin_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void lin2rgb_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void whitepoint_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void deltaE_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void illumwhite_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void illumgray_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void illumpca_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void imcolordiff_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void rgbwide2ycbcr_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void ycbcr2rgbwide_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void rgbwide2xyz_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void xyz2rgbwide_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void cmunique_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void imfuse_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);
void tonemap_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void labeloverlay_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void chromadapt_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void demosaic_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void raw2planar_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void planar2raw_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void xyz2double_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void xyz2uint16_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void brighten_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void contrast_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);

// filter/filter.cpp
void padarray_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void fspecial_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void imfilter_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void imgaussfilt_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void imboxfilt_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void integralBoxFilter_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void integralBoxFilter3_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void modefilt_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void imguidedfilter_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void imdiffusefilt_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void imgaborfilt_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void imnlmfilt_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void locallapfilt_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void imreducehaze_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void fibermetric_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void fsamp2_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);
void ftrans2_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void fwind1_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);
void fwind2_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);
void imboxfilt3_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void convmtx2_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void freqz2_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void imgaussfilt3_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
// filter/filter_design.cpp
void fspecial3_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
// fsamp2 / ftrans2 / fwind1 / fwind2 now live in filter/fir2d.cpp
// (cycle 65). Declared above near fibermetric_reg.
void gabor_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void medfilt3_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void medfilt2_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void imsharpen_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void imnoise_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void stdfilt_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void rangefilt_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void entropyfilt_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void imsmooth_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void wiener2_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void nlfilter_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void colfilt_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void ordfilt2_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void im2col_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void col2im_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void imbilatfilt_reg (Span<const Value>, size_t, Span<Value>, CallContext &);

// contrast/contrast.cpp
void imhist_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void stretchlim_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void imadjust_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void histeq_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void adapthisteq_reg (Span<const Value>, size_t, Span<Value>, CallContext &);

// texture/texture.cpp
void graycomatrix_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void graycoprops_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void adaptthresh_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void imhistmatch_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void imflatfield_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void grayslice_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void entropy_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void wcodemat_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void graythresh_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void otsuthresh_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void multithresh_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void imbinarize_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void imquantize_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);

// morph/morph.cpp
void strel_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);
void imerode_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void imdilate_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void bwmorph_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void bwtraceboundary_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
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
void bwlookup_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void makelut_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void bwmorph3_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void mmgradm_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void bwpack_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);
void bwunpack_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);

// region/region.cpp
void bwlabel_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void bwconncomp_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void labelmatrix_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void cc2bw_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);
void bwpropfilt_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void bwarea_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void bwperim_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void bwareaopen_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void bwboundaries_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void regionprops_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void bwdist_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void bweuler_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void bwareafilt_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void bwselect_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void fchcode_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void roicolor_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);

// object/object.cpp
void imgradientxy_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void imgradient_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void imgradientxyz_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void imgradient3_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void edge_reg        (Span<const Value>, size_t, Span<Value>, CallContext &);
void cornermetric_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void hough_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);
void houghpeaks_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void houghlines_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);

// quality/quality.cpp
void immse_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);
void psnr_reg        (Span<const Value>, size_t, Span<Value>, CallContext &);
void ssim_reg        (Span<const Value>, size_t, Span<Value>, CallContext &);
void mean2_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);
void std2_reg        (Span<const Value>, size_t, Span<Value>, CallContext &);
void corr2_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);
void multissim_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void multissim3_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);

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
void deconvwnr_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void deconvreg_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void edgetaper_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
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
void graydiffweight_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void gradientweight_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void regionfill_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void poly2mask_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void reducepoly_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void roipoly_reg        (Span<const Value>, size_t, Span<Value>, CallContext &);
void graydist_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);
void bwdistgeodesic_reg (Span<const Value>, size_t, Span<Value>, CallContext &);

// geom/geom.cpp
void imresize_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void imresize3_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void imcrop_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void imcrop3_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void imrotate_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void imrotate3_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
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
    reg("type",  "iscolormap",    &image::detail::iscolormap_reg);
    reg("type",  "getrangefromclass",
                                  &image::detail::getrangefromclass_reg);
    reg("type",  "imcast",        &image::detail::imcast_reg);
    reg("type",  "iptnum2ordinal",
                                  &image::detail::iptnum2ordinal_reg);
    reg("type",  "gray2ind",      &image::detail::gray2ind_reg);
    reg("type",  "ind2gray",      &image::detail::ind2gray_reg);
    reg("type",  "ind2rgb",       &image::detail::ind2rgb_reg);
    reg("color", "rgb2gray",      &image::detail::rgb2gray_reg);
    reg("color", "imsplit",       &image::detail::imsplit_reg);

    reg("color", "rgb2hsv",       &image::detail::rgb2hsv_reg);
    reg("color", "hsv2rgb",       &image::detail::hsv2rgb_reg);
    reg("color", "rgb2ycbcr",     &image::detail::rgb2ycbcr_reg);
    reg("color", "ycbcr2rgb",     &image::detail::ycbcr2rgb_reg);
    reg("color", "rgb2ntsc",      &image::detail::rgb2ntsc_reg);
    reg("color", "ntsc2rgb",      &image::detail::ntsc2rgb_reg);
    reg("color", "lab2double",    &image::detail::lab2double_reg);
    reg("color", "lab2single",    &image::detail::lab2single_reg);
    reg("color", "lab2uint8",     &image::detail::lab2uint8_reg);
    reg("color", "lab2uint16",    &image::detail::lab2uint16_reg);
    reg("color", "rgb2xyz",       &image::detail::rgb2xyz_reg);
    reg("color", "xyz2rgb",       &image::detail::xyz2rgb_reg);
    reg("color", "rgb2lab",       &image::detail::rgb2lab_reg);
    reg("color", "rgb2lightness", &image::detail::rgb2lightness_reg);
    reg("color", "rgb2ind",       &image::detail::rgb2ind_reg);
    reg("color", "lab2rgb",       &image::detail::lab2rgb_reg);
    reg("color", "xyz2lab",       &image::detail::xyz2lab_reg);
    reg("color", "lab2xyz",       &image::detail::lab2xyz_reg);
    reg("color", "label2rgb",     &image::detail::label2rgb_reg);
    reg("color", "colorangle",    &image::detail::colorangle_reg);
    reg("color", "wavelength2rgb",
                                  &image::detail::wavelength2rgb_reg);
    reg("color", "colorgradient", &image::detail::colorgradient_reg);
    reg("color", "cmap2gray",     &image::detail::cmap2gray_reg);
    reg("color", "gray",          &image::detail::gray_reg);
    reg("color", "hot",           &image::detail::hot_reg);
    reg("color", "cool",          &image::detail::cool_reg);
    reg("color", "spring",        &image::detail::spring_reg);
    reg("color", "summer",        &image::detail::summer_reg);
    reg("color", "autumn",        &image::detail::autumn_reg);
    reg("color", "winter",        &image::detail::winter_reg);
    reg("color", "copper",        &image::detail::copper_reg);
    reg("color", "pink",          &image::detail::pink_reg);
    reg("color", "hsv",           &image::detail::hsv_cmap_reg);
    reg("color", "flag",          &image::detail::flag_reg);
    reg("color", "prism",         &image::detail::prism_reg);
    reg("color", "lines",         &image::detail::lines_reg);
    reg("color", "bone",          &image::detail::bone_reg);
    reg("color", "white",         &image::detail::white_reg);
    reg("color", "rgb2lin",       &image::detail::rgb2lin_reg);
    reg("color", "lin2rgb",       &image::detail::lin2rgb_reg);
    reg("color", "whitepoint",    &image::detail::whitepoint_reg);
    reg("color", "deltaE",        &image::detail::deltaE_reg);
    reg("color", "illumwhite",    &image::detail::illumwhite_reg);
    reg("color", "illumgray",     &image::detail::illumgray_reg);
    reg("color", "illumpca",      &image::detail::illumpca_reg);
    reg("color", "imcolordiff",   &image::detail::imcolordiff_reg);
    reg("color", "rgbwide2ycbcr", &image::detail::rgbwide2ycbcr_reg);
    reg("color", "ycbcr2rgbwide", &image::detail::ycbcr2rgbwide_reg);
    reg("color", "rgbwide2xyz",   &image::detail::rgbwide2xyz_reg);
    reg("color", "xyz2rgbwide",   &image::detail::xyz2rgbwide_reg);
    reg("color", "cmunique",      &image::detail::cmunique_reg);
    reg("color", "imfuse",        &image::detail::imfuse_reg);
    reg("color", "tonemap",       &image::detail::tonemap_reg);
    reg("color", "labeloverlay",  &image::detail::labeloverlay_reg);
    reg("color", "chromadapt",    &image::detail::chromadapt_reg);
    reg("color", "demosaic",      &image::detail::demosaic_reg);
    reg("color", "raw2planar",    &image::detail::raw2planar_reg);
    reg("color", "planar2raw",    &image::detail::planar2raw_reg);
    reg("color", "xyz2double",    &image::detail::xyz2double_reg);
    reg("color", "xyz2uint16",    &image::detail::xyz2uint16_reg);
    reg("color", "brighten",      &image::detail::brighten_reg);
    reg("color", "contrast",      &image::detail::contrast_reg);

    reg("filter", "padarray",     &image::detail::padarray_reg);
    reg("filter", "fspecial",     &image::detail::fspecial_reg);
    reg("filter", "imfilter",     &image::detail::imfilter_reg);
    reg("filter", "imgaussfilt",  &image::detail::imgaussfilt_reg);
    reg("filter", "imboxfilt",    &image::detail::imboxfilt_reg);
    reg("filter", "integralBoxFilter", &image::detail::integralBoxFilter_reg);
    reg("filter", "integralBoxFilter3", &image::detail::integralBoxFilter3_reg);
    reg("filter", "modefilt",     &image::detail::modefilt_reg);
    reg("filter", "imguidedfilter", &image::detail::imguidedfilter_reg);
    reg("filter", "imdiffusefilt",  &image::detail::imdiffusefilt_reg);
    reg("filter", "imgaborfilt",    &image::detail::imgaborfilt_reg);
    reg("filter", "imnlmfilt",      &image::detail::imnlmfilt_reg);
    reg("filter", "locallapfilt",   &image::detail::locallapfilt_reg);
    reg("filter", "imreducehaze",   &image::detail::imreducehaze_reg);
    reg("filter", "fibermetric",    &image::detail::fibermetric_reg);
    reg("filter", "fsamp2",         &image::detail::fsamp2_reg);
    reg("filter", "ftrans2",        &image::detail::ftrans2_reg);
    reg("filter", "fwind1",         &image::detail::fwind1_reg);
    reg("filter", "fwind2",         &image::detail::fwind2_reg);
    reg("filter", "imboxfilt3",   &image::detail::imboxfilt3_reg);
    reg("filter", "convmtx2",     &image::detail::convmtx2_reg);
    reg("filter", "freqz2",       &image::detail::freqz2_reg);
    reg("filter", "imgaussfilt3", &image::detail::imgaussfilt3_reg);
    reg("filter", "fspecial3",    &image::detail::fspecial3_reg);
    reg("filter", "gabor",        &image::detail::gabor_reg);
    reg("filter", "medfilt3",     &image::detail::medfilt3_reg);
    reg("filter", "medfilt2",     &image::detail::medfilt2_reg);
    reg("filter", "imsharpen",    &image::detail::imsharpen_reg);
    reg("filter", "imnoise",      &image::detail::imnoise_reg);
    reg("filter", "stdfilt",      &image::detail::stdfilt_reg);
    reg("filter", "rangefilt",    &image::detail::rangefilt_reg);
    reg("filter", "entropyfilt",  &image::detail::entropyfilt_reg);
    reg("filter", "imsmooth",     &image::detail::imsmooth_reg);
    reg("filter", "wiener2",      &image::detail::wiener2_reg);
    reg("filter", "nlfilter",     &image::detail::nlfilter_reg);
    reg("filter", "colfilt",      &image::detail::colfilt_reg);
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
    reg("contrast", "adapthisteq",&image::detail::adapthisteq_reg);
    reg("texture",  "graycomatrix", &image::detail::graycomatrix_reg);
    reg("texture",  "graycoprops",  &image::detail::graycoprops_reg);
    reg("contrast", "adaptthresh",&image::detail::adaptthresh_reg);
    reg("contrast", "imhistmatch",&image::detail::imhistmatch_reg);
    reg("contrast", "imflatfield",&image::detail::imflatfield_reg);
    reg("contrast", "grayslice",  &image::detail::grayslice_reg);
    reg("contrast", "entropy",    &image::detail::entropy_reg);
    reg("contrast", "wcodemat",   &image::detail::wcodemat_reg);

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
    reg("morph", "bwmorph",   &image::detail::bwmorph_reg);
    reg("morph", "bwtraceboundary", &image::detail::bwtraceboundary_reg);
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
    reg("morph", "bwlookup",      &image::detail::bwlookup_reg);
    reg("morph", "makelut",       &image::detail::makelut_reg);
    reg("morph", "bwmorph3",      &image::detail::bwmorph3_reg);
    reg("morph", "mmgradm",       &image::detail::mmgradm_reg);
    reg("morph", "bwpack",        &image::detail::bwpack_reg);
    reg("morph", "bwunpack",      &image::detail::bwunpack_reg);

    reg("region", "bwlabel",      &image::detail::bwlabel_reg);
    reg("region", "bwconncomp",   &image::detail::bwconncomp_reg);
    reg("region", "labelmatrix",  &image::detail::labelmatrix_reg);
    reg("region", "cc2bw",        &image::detail::cc2bw_reg);
    reg("region", "bwpropfilt",   &image::detail::bwpropfilt_reg);
    reg("region", "bwarea",       &image::detail::bwarea_reg);
    reg("region", "bwperim",      &image::detail::bwperim_reg);
    reg("region", "bwareaopen",   &image::detail::bwareaopen_reg);
    reg("region", "bwboundaries", &image::detail::bwboundaries_reg);
    reg("region", "regionprops",  &image::detail::regionprops_reg);
    reg("region", "bwdist",       &image::detail::bwdist_reg);
    reg("region", "bweuler",      &image::detail::bweuler_reg);
    reg("region", "bwareafilt",   &image::detail::bwareafilt_reg);
    reg("region", "bwselect",     &image::detail::bwselect_reg);
    reg("region", "fchcode",      &image::detail::fchcode_reg);
    reg("region", "roicolor",     &image::detail::roicolor_reg);

    reg("object", "imgradientxy", &image::detail::imgradientxy_reg);
    reg("object", "imgradient",   &image::detail::imgradient_reg);
    reg("object", "imgradientxyz",&image::detail::imgradientxyz_reg);
    reg("object", "imgradient3",  &image::detail::imgradient3_reg);
    reg("object", "edge",         &image::detail::edge_reg);
    reg("object", "cornermetric", &image::detail::cornermetric_reg);
    reg("object", "hough",        &image::detail::hough_reg);
    reg("object", "houghpeaks",   &image::detail::houghpeaks_reg);
    reg("object", "houghlines",   &image::detail::houghlines_reg);

    reg("quality", "immse", &image::detail::immse_reg);
    reg("quality", "psnr",  &image::detail::psnr_reg);
    reg("quality", "ssim",  &image::detail::ssim_reg);
    reg("quality", "mean2", &image::detail::mean2_reg);
    reg("quality", "std2",  &image::detail::std2_reg);
    reg("quality", "corr2", &image::detail::corr2_reg);
    reg("quality", "multissim", &image::detail::multissim_reg);
    reg("quality", "multissim3", &image::detail::multissim3_reg);

    reg("transform", "dct2",          &image::detail::dct2_reg);
    reg("transform", "idct2",         &image::detail::idct2_reg);
    reg("transform", "dctmtx",        &image::detail::dctmtx_reg);
    reg("transform", "integralImage", &image::detail::integralImage_reg);
    reg("transform", "integralImage3",&image::detail::integralImage3_reg);
    reg("transform", "checkerboard",  &image::detail::checkerboard_reg);
    reg("transform", "normxcorr2",    &image::detail::normxcorr2_reg);
    reg("transform", "psf2otf",       &image::detail::psf2otf_reg);
    reg("transform", "otf2psf",       &image::detail::otf2psf_reg);
    reg("transform", "deconvwnr",     &image::detail::deconvwnr_reg);
    reg("transform", "deconvreg",     &image::detail::deconvreg_reg);
    reg("transform", "edgetaper",     &image::detail::edgetaper_reg);
    reg("transform", "fftconv2",      &image::detail::fftconv2_reg);
    reg("transform", "bestblk",       &image::detail::bestblk_reg);
    reg("transform", "phantom",       &image::detail::phantom_reg);

    reg("io", "imread",  &image::detail::imread_reg);
    reg("io", "imwrite", &image::detail::imwrite_reg);
    reg("io", "imfinfo", &image::detail::imfinfo_reg);

    reg("geom", "imresize",    &image::detail::imresize_reg);
    reg("geom", "imresize3",   &image::detail::imresize3_reg);
    reg("geom", "imcrop",      &image::detail::imcrop_reg);
    reg("geom", "imcrop3",     &image::detail::imcrop3_reg);
    reg("geom", "imrotate",    &image::detail::imrotate_reg);
    reg("geom", "imrotate3",   &image::detail::imrotate3_reg);
    reg("geom", "imtranslate", &image::detail::imtranslate_reg);
    reg("geom", "impyramid",   &image::detail::impyramid_reg);
    reg("geom", "axes2pix",    &image::detail::axes2pix_reg);

    reg("segment", "dice",          &image::detail::dice_reg);
    reg("segment", "jaccard",       &image::detail::jaccard_reg);
    reg("segment", "boundarymask",  &image::detail::boundarymask_reg);
    reg("segment", "label2idx",     &image::detail::label2idx_reg);
    reg("segment", "grayconnected", &image::detail::grayconnected_reg);
    reg("segment", "imoverlay",     &image::detail::imoverlay_reg);
    reg("segment", "graydiffweight",&image::detail::graydiffweight_reg);
    reg("segment", "gradientweight",&image::detail::gradientweight_reg);
    reg("segment", "regionfill",    &image::detail::regionfill_reg);
    reg("segment", "poly2mask",     &image::detail::poly2mask_reg);
    reg("segment", "reducepoly",    &image::detail::reducepoly_reg);
    reg("segment", "roipoly",       &image::detail::roipoly_reg);
    reg("segment", "graydist",      &image::detail::graydist_reg);
    reg("segment", "bwdistgeodesic",&image::detail::bwdistgeodesic_reg);
}

} // namespace numkit
