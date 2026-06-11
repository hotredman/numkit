#pragma once
//
// value_json.hpp — per-cell JSON serialization of a numeric Value.
//
// Single source of truth for how one matrix cell is turned into a JSON
// token in the IDE's Variable viewer. Shared by the WASM repl_bindings
// serializers (emitMatrixDataArray / getVarTileJSON / valuePreview) so
// that every display path renders integer / single / double / logical
// values identically — and so the logic stays testable in the native
// gtest (repl_bindings.cpp itself is an emscripten-only translation unit).
//
// CHAR and COMPLEX have their own string framing at the call sites and are
// intentionally NOT handled here.

#include <numkit/value/value.hpp>
#include <numkit/value/value_type.hpp>

#include <string>
#include <sstream>
#include <cmath>

namespace numkit {

// True for the real scalar/bool element classes numericCellJSON renders:
// DOUBLE, SINGLE, LOGICAL, and every signed/unsigned integer class.
// (COMPLEX / CHAR / CELL / STRUCT / FUNC_HANDLE / STRING are excluded —
// callers frame those separately.)
inline bool isRealNumericCell(ValueType t)
{
    return t == ValueType::DOUBLE || t == ValueType::SINGLE
        || t == ValueType::LOGICAL || isIntegerType(t);
}

// JSON token for the cell at column-major linear index `i`.
//   integer classes → exact decimal, no fractional part   (e.g. 200)
//   LOGICAL         → true / false
//   DOUBLE / SINGLE → NaN → null, +Inf → "Inf", -Inf → "-Inf",
//                     finite → full 17-significant-digit decimal
// Precondition: isRealNumericCell(val.type()) and i < val.numel().
inline std::string numericCellJSON(const Value &val, std::size_t i)
{
    switch (val.type()) {
    case ValueType::INT8:    return std::to_string(static_cast<int>(val.int8Data()[i]));
    case ValueType::INT16:   return std::to_string(static_cast<int>(val.int16Data()[i]));
    case ValueType::INT32:   return std::to_string(val.int32Data()[i]);
    case ValueType::INT64:   return std::to_string(static_cast<long long>(val.int64Data()[i]));
    case ValueType::UINT8:   return std::to_string(static_cast<unsigned>(val.uint8Data()[i]));
    case ValueType::UINT16:  return std::to_string(static_cast<unsigned>(val.uint16Data()[i]));
    case ValueType::UINT32:  return std::to_string(val.uint32Data()[i]);
    case ValueType::UINT64:  return std::to_string(static_cast<unsigned long long>(val.uint64Data()[i]));
    case ValueType::LOGICAL: return val.logicalData()[i] ? "true" : "false";
    default: {  // DOUBLE / SINGLE
        double v = val.elemAsDouble(i);
        if (std::isnan(v)) return "null";
        if (std::isinf(v)) return v > 0 ? "\"Inf\"" : "\"-Inf\"";
        std::ostringstream s;
        s.precision(17);
        s << v;
        return s.str();
    }
    }
}

}  // namespace numkit
