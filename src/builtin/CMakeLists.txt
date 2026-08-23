# src/builtin/CMakeLists.txt

list(APPEND NUMKIT_BUILTIN_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/builtin.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ops/binary_ops.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ops/unary_ops.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/elfun/trigonometry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/elfun/exponents.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/elfun/complex.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/elfun/rounding.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/elmat/creation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/elmat/manipulation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/matfun/matfun.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/datafun/reductions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/datafun/random.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/datafun/sets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/specfun/gamma_beta.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/specfun/erf.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/specfun/bessel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/specfun/combinatorics.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/polyfun/polynomials.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/polyfun/interpolation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/polyfun/integration.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/strfun/conversion.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/strfun/manipulation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/strfun/predicates.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/strfun/regex.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/timefun.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/datatypes.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/iofun.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/general.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/lang.cpp
)

if(NUMKIT_WITH_SIMD)
    list(APPEND NUMKIT_BUILTIN_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/elfun/trig_highway.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/elfun/trig_recip_highway.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/elfun/exp_log_highway.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/elfun/rounding_highway.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/elfun/abs_highway.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/elfun/mod_highway.cpp
    )
else()
    list(APPEND NUMKIT_BUILTIN_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/elfun/trig_portable.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/elfun/trig_recip_portable.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/elfun/exp_log_portable.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/elfun/rounding_portable.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/elfun/abs_portable.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/elfun/mod_portable.cpp
    )
endif()

list(APPEND NUMKIT_BUILTIN_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/builtin/builtin.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/builtin/ops.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/builtin/elmat.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/builtin/elfun.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/builtin/matfun.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/builtin/datafun.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/builtin/specfun.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/builtin/polyfun.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/builtin/strfun.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/builtin/timefun.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/builtin/datatypes.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/builtin/iofun.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/builtin/general.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/builtin/lang.hpp
)

list(APPEND NUMKIT_BUILTIN_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

set(NUMKIT_BUILTIN_SOURCES      ${NUMKIT_BUILTIN_SOURCES}      PARENT_SCOPE)
set(NUMKIT_BUILTIN_HEADERS      ${NUMKIT_BUILTIN_HEADERS}      PARENT_SCOPE)
set(NUMKIT_BUILTIN_INCLUDE_DIRS ${NUMKIT_BUILTIN_INCLUDE_DIRS} PARENT_SCOPE)
