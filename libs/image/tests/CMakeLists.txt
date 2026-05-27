# libs/image/tests/CMakeLists.txt
#
# Image Toolbox unit tests. Each new function in /loop adds a TU here
# via target_sources(numkit_gtest PRIVATE ...). Hand-runnable .m smokes
# live in tests/smoke/ (next to this file) and are not compiled.

target_sources(numkit_gtest PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/wcodemat_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/image_batch1_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/image_batch2_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/image_batch3_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/image_batch4_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/image_batch5_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/color_extras_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_design_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/adapthisteq_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/adapthisteq_cpp_api_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/graycomatrix_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/bwmorph_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/illum_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/illumpca_imcolordiff_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/otf2psf_rgbwide_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ycbcr2rgbwide_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ind2gray_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imcrop3_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/graydiffweight_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/nlfilter_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/colfilt_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cmunique_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/deconvwnr_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/edgetaper_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imgradientxyz_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/deconvreg_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/gradientweight_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/regionfill_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/poly2mask_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/roipoly_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imfuse_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cornermetric_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/hough_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/houghlines_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/bwtraceboundary_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imguidedfilter_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/imdiffusefilt_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/graydist_test.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/bwdistgeodesic_test.cpp
)
