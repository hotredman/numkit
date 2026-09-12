# toolboxes/linalg/CMakeLists.txt
#
# Linear Algebra Toolbox. User-facing linalg surface (lu, qr, chol, svd,
# eig, det, inv, pinv, norm, expm, …). Depends on toolboxes/builtin (reuses
# the internal `la_solve` kernel and `Value` plumbing).

add_subdirectory(src)  # exports LINALGLIB_SOURCES via PARENT_SCOPE

list(APPEND NK_LIBS_SOURCES ${LINALGLIB_SOURCES})
list(APPEND NK_LIBS_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)

list(APPEND NK_LIBS_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/linalg/library.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/linalg/vector_ops.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/linalg/norms.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/linalg/properties.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/linalg/decompositions.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/linalg/pseudo_subspace.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/linalg/balance.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/linalg/ldl.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/linalg/eig.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/linalg/matrix_functions.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/linalg/predicates.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/linalg/solvers.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/linalg/misc.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/linalg/page_ops.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/linalg/schur_convert.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/linalg/ordschur.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/linalg/ordqz.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/linalg/qz.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/linalg/gsvd.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/linalg/eigs.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/nk/linalg/svd_sketch.hpp
)

set(NK_LIBS_SOURCES      ${NK_LIBS_SOURCES}      PARENT_SCOPE)
set(NK_LIBS_HEADERS      ${NK_LIBS_HEADERS}      PARENT_SCOPE)
set(NK_LIBS_INCLUDE_DIRS ${NK_LIBS_INCLUDE_DIRS} PARENT_SCOPE)
