// src/MLabMType.cpp
#include "MMType.hpp"

namespace numkit {

const char *mtypeName(MType t)
{
    switch (t) {
    case MType::EMPTY:        return "empty";
    case MType::DOUBLE:       return "double";
    case MType::COMPLEX:      return "complex";
    case MType::LOGICAL:      return "logical";
    case MType::CHAR:         return "char";
    case MType::CELL:         return "cell";
    case MType::STRUCT:       return "struct";
    case MType::FUNC_HANDLE:  return "function_handle";
    case MType::INT8:         return "int8";
    case MType::INT16:        return "int16";
    case MType::INT32:        return "int32";
    case MType::INT64:        return "int64";
    case MType::UINT8:        return "uint8";
    case MType::UINT16:       return "uint16";
    case MType::UINT32:       return "uint32";
    case MType::UINT64:       return "uint64";
    case MType::SINGLE:       return "single";
    case MType::STRING:       return "string";
    }
    return "unknown";
}

size_t elementSize(MType t)
{
    switch (t) {
    case MType::DOUBLE:  return 8;
    case MType::COMPLEX: return 16;
    case MType::LOGICAL: return 1;
    case MType::CHAR:    return 1;
    case MType::INT8:    return 1;
    case MType::INT16:   return 2;
    case MType::INT32:   return 4;
    case MType::INT64:   return 8;
    case MType::UINT8:   return 1;
    case MType::UINT16:  return 2;
    case MType::UINT32:  return 4;
    case MType::UINT64:  return 8;
    case MType::SINGLE:  return 4;
    default:             return 0;
    }
}

bool isIntegerType(MType t)
{
    return t == MType::INT8 || t == MType::INT16 || t == MType::INT32 || t == MType::INT64
        || t == MType::UINT8 || t == MType::UINT16 || t == MType::UINT32 || t == MType::UINT64;
}

bool isFloatType(MType t)
{
    return t == MType::DOUBLE || t == MType::SINGLE;
}

} // namespace numkit
