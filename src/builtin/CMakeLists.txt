# src/builtin/CMakeLists.txt

list(APPEND NK_BUILTIN_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ops/binary_ops.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ops/unary_ops.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ops/int_math.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/elfun/trigonometry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/elfun/exponents.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/elfun/complex.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/elfun/rounding.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/elmat/matrix.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/elmat/manip.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/elmat/nd_manip.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/matfun/matfun.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/datafun/reductions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/datafun/sets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/datafun/group.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/datafun/accum.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/specfun/special.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/specfun/combinatorics.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/polyfun/polynomials.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/polyfun/interpolation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/polyfun/integration.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/polyfun/geometry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/strfun/regex.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/strfun/strings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/strfun/format.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/strfun/print.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/strfun/scan.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/timefun/clock.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/timefun/dates.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/datatypes/types.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/iofun/formatted_io.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lang/environment.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lang/keywords.cpp
)

if(NK_WITH_SIMD)
    list(APPEND NK_BUILTIN_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/elfun/trig_highway.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/elfun/trig_recip_highway.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/elfun/exp_log_highway.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/elfun/rounding_highway.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/elfun/abs_highway.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/elfun/mod_highway.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/specfun/special_highway.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/datafun/cumsum_highway.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/datafun/logical_reductions_highway.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/datafun/var_reduction_highway.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/datafun/isfinite_highway.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/datatypes/casts_highway.cpp
    )
else()
    list(APPEND NK_BUILTIN_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/elfun/trig_portable.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/elfun/trig_recip_portable.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/elfun/exp_log_portable.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/elfun/rounding_portable.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/elfun/abs_portable.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/elfun/mod_portable.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/specfun/special_portable.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/datafun/cumsum_portable.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/datafun/logical_reductions_portable.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/datafun/var_reduction_portable.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/datafun/isfinite_portable.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/datatypes/casts_portable.cpp
    )
endif()

list(APPEND NK_BUILTIN_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/builtin/builtin.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/builtin/ops.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/builtin/elmat.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/builtin/elfun.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/builtin/matfun.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/builtin/datafun.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/builtin/specfun.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/builtin/polyfun.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/builtin/strfun.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/builtin/timefun.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/builtin/datatypes.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/builtin/iofun.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/builtin/lang.hpp
)

list(APPEND NK_BUILTIN_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set(NK_BUILTIN_SOURCES      ${NK_BUILTIN_SOURCES}      PARENT_SCOPE)
set(NK_BUILTIN_HEADERS      ${NK_BUILTIN_HEADERS}      PARENT_SCOPE)
set(NK_BUILTIN_INCLUDE_DIRS ${NK_BUILTIN_INCLUDE_DIRS} PARENT_SCOPE)
