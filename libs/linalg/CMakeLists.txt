# libs/linalg/CMakeLists.txt
#
# Linear Algebra Toolbox. User-facing linalg surface (lu, qr, chol, svd,
# eig, det, inv, pinv, norm, expm, …). Depends on libs/builtin (reuses
# the internal `la_solve` kernel and `Value` plumbing).

add_subdirectory(src)  # exports LINALGLIB_SOURCES via PARENT_SCOPE

list(APPEND NUMKIT_LIBS_SOURCES ${LINALGLIB_SOURCES})
list(APPEND NUMKIT_LIBS_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)

list(APPEND NUMKIT_LIBS_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/linalg/library.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/linalg/vector_ops.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/linalg/norms.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/linalg/properties.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/linalg/decompositions.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/linalg/pseudo_subspace.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/linalg/balance.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/linalg/ldl.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/linalg/eig.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/numkit/linalg/matrix_functions.hpp
)

set(NUMKIT_LIBS_SOURCES      ${NUMKIT_LIBS_SOURCES}      PARENT_SCOPE)
set(NUMKIT_LIBS_HEADERS      ${NUMKIT_LIBS_HEADERS}      PARENT_SCOPE)
set(NUMKIT_LIBS_INCLUDE_DIRS ${NUMKIT_LIBS_INCLUDE_DIRS} PARENT_SCOPE)
