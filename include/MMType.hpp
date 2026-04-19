// include/MMType.hpp
#pragma once

#include <cstddef>
#include <cstdint>

namespace numkit::m {

// ============================================================
// MType — element type enum
// ============================================================
enum class MType : uint8_t {
    EMPTY,
    DOUBLE,
    COMPLEX,
    LOGICAL,
    CHAR,
    CELL,
    STRUCT,
    FUNC_HANDLE,
    INT8,
    INT16,
    INT32,
    INT64,
    UINT8,
    UINT16,
    UINT32,
    UINT64,
    SINGLE,
    STRING
};

const char *mtypeName(MType t);
size_t elementSize(MType t);
bool isIntegerType(MType t);
bool isFloatType(MType t); // double or single

} // namespace numkit::m
