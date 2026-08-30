// core/include/numkit/value/value_type.hpp
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
    STRING,
    OBJECT,  // class instance (builtin now, user classdef later); see OBJECT_MODEL.md
    CSL      // comma-separated list — a TRANSIENT value-list produced by c{:} / c{idx} /
             // s.field expansion that splicing contexts (call args, [..], {..}, multi-
             // assign) flatten. Never stored in a variable; a single-value context must
             // collapse it (1 elem -> the elem; N -> "too many values"). Reuses
             // HeapObject::cellData for storage. See dev-docs/memory/CSL_FIRST_CLASS.md.
};

const char *mtypeName(ValueType t);
size_t elementSize(ValueType t);
bool isIntegerType(ValueType t);
bool isFloatType(ValueType t); // double or single

} // namespace numkit
