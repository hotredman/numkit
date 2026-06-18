set(CODEGENLIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/type_lattice.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/inference.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/indexing.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/transfer/registry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/transfer/constructors.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/transfer/elementwise.cpp
)
set(CODEGENLIB_SOURCES ${CODEGENLIB_SOURCES} PARENT_SCOPE)
