set(IMAGELIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/library.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/arithmetic/arithmetic.cpp           # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/arithmetic/arithmetic_reg.cpp       # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/type_convert/type_convert.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/color/color.cpp        # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/color/color_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/color/color_extras.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/color/chromadapt.cpp   # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/color/chromadapt_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/color/illum.cpp        # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/color/illum_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/color/demosaic.cpp     # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/color/demosaic_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/color/raw_planar.cpp   # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/color/raw_planar_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/filter/filter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/filter/filter_design.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/filter/filter_design_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/filter/locallapfilt.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/filter/locallapfilt_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/filter/imreducehaze.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/filter/imreducehaze_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/filter/fibermetric.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/filter/fibermetric_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/filter/fir2d.cpp       # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/filter/fir2d_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/contrast/contrast.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/morph/morph.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/region/region.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/object/object.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/quality/quality.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/transform/transform.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/io/stb_impl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/io/io.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/io/tiff_reader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/io/tiff_writer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/geom/geom.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/segment/segment.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/region/bwpropfilt.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/texture/texture.cpp
)

set(IMAGELIB_SOURCES ${IMAGELIB_SOURCES} PARENT_SCOPE)
