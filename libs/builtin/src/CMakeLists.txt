set(STDLIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/MStdLibrary.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MStdBinaryOps.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MStdUnaryOps.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MStdMath.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MStdMatrix.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MStdTypes.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MStdCellStruct.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MStdStrings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MStdComplex.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MStdFormat.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MStdDiagnostics.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MStdPrint.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MStdFileIO.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MStdScan.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MStdCsv.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MStdEnv.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MStdSaveLoad.cpp
)

# Backend-split functions: one definition per TU, picked at configure
# time via NUMKIT_WITH_SIMD. Keep these lines 1-per-function so the
# diff is readable when a new function gets a SIMD backend.
if(NUMKIT_WITH_SIMD)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/backends/MStdAbs_simd.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/backends/MStdTranscendental_simd.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/backends/MStdBinaryOps_simd.cpp)
else()
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/backends/MStdAbs_portable.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/backends/MStdTranscendental_portable.cpp)
    list(APPEND STDLIB_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/backends/MStdBinaryOps_portable.cpp)
endif()

set(STDLIB_SOURCES ${STDLIB_SOURCES} PARENT_SCOPE)
