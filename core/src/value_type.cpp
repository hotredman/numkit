// core/src/value_type.cpp
#include <numkit/core/value_type.hpp>

namespace numkit {

const char *mtypeName(ValueType t)
{
    switch (t) {
    case ValueType::EMPTY:        return "empty";
    case ValueType::DOUBLE:       return "double";
    case ValueType::COMPLEX:      return "complex";
    case ValueType::LOGICAL:      return "logical";
    case ValueType::CHAR:         return "char";
    case ValueType::CELL:         return "cell";
    case ValueType::STRUCT:       return "struct";
    case ValueType::FUNC_HANDLE:  return "function_handle";
    case ValueType::INT8:         return "int8";
    case ValueType::INT16:        return "int16";
    case ValueType::INT32:        return "int32";
    case ValueType::INT64:        return "int64";
    case ValueType::UINT8:        return "uint8";
    case ValueType::UINT16:       return "uint16";
    case ValueType::UINT32:       return "uint32";
    case ValueType::UINT64:       return "uint64";
    case ValueType::SINGLE:       return "single";
    case ValueType::STRING:       return "string";
    }
    return "unknown";
}

size_t elementSize(ValueType t)
{
    switch (t) {
    case ValueType::DOUBLE:  return 8;
    case ValueType::COMPLEX: return 16;
    case ValueType::LOGICAL: return 1;
    case ValueType::CHAR:    return 1;
    case ValueType::INT8:    return 1;
    case ValueType::INT16:   return 2;
    case ValueType::INT32:   return 4;
    case ValueType::INT64:   return 8;
    case ValueType::UINT8:   return 1;
    case ValueType::UINT16:  return 2;
    case ValueType::UINT32:  return 4;
    case ValueType::UINT64:  return 8;
    case ValueType::SINGLE:  return 4;
    default:             return 0;
    }
}

bool isIntegerType(ValueType t)
{
    return t == ValueType::INT8 || t == ValueType::INT16 || t == ValueType::INT32 || t == ValueType::INT64
        || t == ValueType::UINT8 || t == ValueType::UINT16 || t == ValueType::UINT32 || t == ValueType::UINT64;
}

bool isFloatType(ValueType t)
{
    return t == ValueType::DOUBLE || t == ValueType::SINGLE;
}

} // namespace numkit
