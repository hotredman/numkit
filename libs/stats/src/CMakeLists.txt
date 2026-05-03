set(STATSLIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/library.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/descriptive/descriptive.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/descriptive/descriptive_extras.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/moments/moments.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/nan_aware/nan_aware.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/moving/moving.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/normal.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/chi2.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/students_t.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/fisher_f.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/beta.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/gamma_dist.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/exponential.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/uniform.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/lognormal.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/weibull.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/rayleigh.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/poisson.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/binomial.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/unid.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/geometric.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/negbin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/distributions/hypergeom.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cluster/distance.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cluster/kmeans.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cluster/kmedoids.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cluster/linkage.cpp
)

# Backend-split functions: one definition per TU, picked at configure
# time via NUMKIT_WITH_SIMD. nan_reductions kernels feed nansum / nanmean
# in nan_aware.cpp.
if(NUMKIT_WITH_SIMD)
    list(APPEND STATSLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/nan_aware/backends/nan_reductions_simd.cpp)
else()
    list(APPEND STATSLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/nan_aware/backends/nan_reductions_portable.cpp)
endif()

set(STATSLIB_SOURCES ${STATSLIB_SOURCES} PARENT_SCOPE)
