set(WAVELETLIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/library.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/filter/wfilters.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/filter/qmf.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/filter/families.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dwt/dwt.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dwt/multilevel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dwt/dwt2.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dwt/dyad.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dwt/wkeep_wextend.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/dwt/haart.cpp        # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/dwt/ihaart.cpp       # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/dwt/haar_reg.cpp     # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/dwt/wrcoef.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/denoise/denoise.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shape/shape.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shape/gauss.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/swt/swt.cpp
)

set(WAVELETLIB_SOURCES ${WAVELETLIB_SOURCES} PARENT_SCOPE)
