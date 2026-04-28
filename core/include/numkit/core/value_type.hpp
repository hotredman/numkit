// core/include/numkit/core/value_type.hpp
#pragma once

#include <cstddef>
#include <cstdint>

namespace numkit {

// ============================================================
// ValueType — element type enum
// ============================================================
enum class ValueType : uint8_t {
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

const char *mtypeName(ValueType t);
size_t elementSize(ValueType t);
bool isIntegerType(ValueType t);
bool isFloatType(ValueType t); // double or single

} // namespace numkit
