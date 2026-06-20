set(CODEGENLIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/type_lattice.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/inference.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/indexing.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/emitter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/aot.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/driver.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/monomorphize.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/classinfo.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/transfer/registry.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/transfer/constructors.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/transfer/elementwise.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/transfer/shape.cpp
)
set(CODEGENLIB_SOURCES ${CODEGENLIB_SOURCES} PARENT_SCOPE)
