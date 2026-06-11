# toolboxes/stats/CMakeLists.txt
#
# Statistics Toolbox. Depends on builtin (base layer).

add_subdirectory(src)  # exports STATSLIB_SOURCES via PARENT_SCOPE

list(APPEND NUMKIT_LIBS_SOURCES ${STATSLIB_SOURCES})
list(APPEND NUMKIT_LIBS_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)

list(APPEND NUMKIT_LIBS_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/stats/library.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/stats/descriptive/descriptive.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/stats/moments/moments.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/stats/moving/moving.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/stats/nan_aware/nan_aware.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/stats/fit/fit.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/stats/mvdist/mvdist.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/stats/anova/anova.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/stats/regress/regress.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/stats/lda/lda.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/stats/spline/spline.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/stats/qmc/qmc.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/stats/sampling/lhs.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/stats/copulas/copulas.hpp
)

set(NUMKIT_LIBS_SOURCES      ${NUMKIT_LIBS_SOURCES}      PARENT_SCOPE)
set(NUMKIT_LIBS_HEADERS      ${NUMKIT_LIBS_HEADERS}      PARENT_SCOPE)
set(NUMKIT_LIBS_INCLUDE_DIRS ${NUMKIT_LIBS_INCLUDE_DIRS} PARENT_SCOPE)
