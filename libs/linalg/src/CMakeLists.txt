# libs/linalg/src/CMakeLists.txt
#
# Linear Algebra Toolbox sources. Files are added per migration group
# as functions move out of libs/builtin/src/language/arrays/.

set(LINALGLIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/library.cpp
)

set(LINALGLIB_SOURCES ${LINALGLIB_SOURCES} PARENT_SCOPE)
