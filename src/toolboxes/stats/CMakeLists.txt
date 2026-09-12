# toolboxes/stats/CMakeLists.txt
#
# Statistics Toolbox. Depends on builtin (base layer).

add_subdirectory(src)  # exports STATSLIB_SOURCES via PARENT_SCOPE

list(APPEND NK_LIBS_SOURCES ${STATSLIB_SOURCES})
list(APPEND NK_LIBS_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)

list(APPEND NK_LIBS_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/stats/library.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/stats/descriptive/descriptive.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/stats/moments/moments.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/stats/moving/moving.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/stats/nan_aware/nan_aware.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/stats/fit/fit.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/stats/mvdist/mvdist.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/stats/anova/anova.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/stats/regress/regress.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/stats/lda/lda.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/stats/spline/spline.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/stats/qmc/qmc.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/stats/sampling/lhs.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/stats/copulas/copulas.hpp
)

set(NK_LIBS_SOURCES      ${NK_LIBS_SOURCES}      PARENT_SCOPE)
set(NK_LIBS_HEADERS      ${NK_LIBS_HEADERS}      PARENT_SCOPE)
set(NK_LIBS_INCLUDE_DIRS ${NK_LIBS_INCLUDE_DIRS} PARENT_SCOPE)
