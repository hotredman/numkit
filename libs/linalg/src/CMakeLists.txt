# libs/linalg/src/CMakeLists.txt
#
# Linear Algebra Toolbox sources. Files are added per migration group
# as functions move out of libs/builtin/src/language/arrays/.

set(LINALGLIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/library.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/vector_ops.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/norms.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/properties.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/decompositions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/pseudo_subspace.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/balance.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ldl.cpp
)

set(LINALGLIB_SOURCES ${LINALGLIB_SOURCES} PARENT_SCOPE)
