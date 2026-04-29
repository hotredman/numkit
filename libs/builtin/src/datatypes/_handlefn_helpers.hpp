// libs/builtin/src/datatypes/_handlefn_helpers.hpp
//
// Shared cellfun / structfun helpers — built-in fast-path dispatch for
// common function handles (numel, length, isnumeric, sum, ...) plus the
// callback fallback for anonymous handles. Header-only so cell.cpp and
// struct.cpp can share it without risking ODR violations.
//
// Everything is `inline` to keep this header-only.

#pragma once

#include <numkit/builtin/math/elementary/reductions.hpp>   // sum / prod / mean
#include <numkit/builtin/lang/arrays/matrix.hpp>  // length / ndims
#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>
#include <numkit/core/value.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <vector>

namespace numkit::builtin::detail::handlefn {

using ::numkit::Engine;

enum class BuiltinFn {
    // shape
    Numel, Length, Ndims, IsEmpty,
    // type predicates
    IsNumeric, IsChar, IsLogical, IsCell, IsStruct, IsReal,
    IsNan, IsInf, IsFinite,
    // numeric reductions
    Sum, Prod, Mean,
    // text
    ClassName,
};

inline bool builtinReturnsLogical(BuiltinFn f)
{
    switch (f) {
    case BuiltinFn::IsEmpty:
    case BuiltinFn::IsNumeric: case BuiltinFn::IsChar:
    case BuiltinFn::IsLogical: case BuiltinFn::IsCell:
    case BuiltinFn::IsStruct:  case BuiltinFn::IsReal:
    case BuiltinFn::IsNan: case BuiltinFn::IsInf: case BuiltinFn::IsFinite:
        return true;
    default:
        return false;
    }
}

inline bool builtinReturnsString(BuiltinFn f)
{
    return f == BuiltinFn::ClassName;
}

inline std::string lowerName(const std::string &s)
{
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return r;
}

// Returns true with `out` set if the handle is a recognised built-in.
// Returns false otherwise (caller routes through engine callback).
// Accepts both raw funcHandle and the {handle, captures...} closure
// cell that VM produces for anonymous functions.
inline bool tryParseBuiltinHandle(const Value &h, BuiltinFn &out, const char *fn)
{
    const Value *bare = &h;
    if (h.isCell() && h.numel() >= 1 && h.cellAt(0).isFuncHandle())
        bare = &h.cellAt(0);
    if (!bare->isFuncHandle())
        throw Error(std::string(fn) + ": fn argument must be a function handle",
                     0, 0, fn, "", std::string("m:") + fn + ":fnType");
    const std::string s = lowerName(bare->funcHandleName());
    if (s == "numel")     { out = BuiltinFn::Numel;     return true; }
    if (s == "length")    { out = BuiltinFn::Length;    return true; }
    if (s == "ndims")     { out = BuiltinFn::Ndims;     return true; }
    if (s == "isempty")   { out = BuiltinFn::IsEmpty;   return true; }
    if (s == "isnumeric") { out = BuiltinFn::IsNumeric; return true; }
    if (s == "ischar")    { out = BuiltinFn::IsChar;    return true; }
    if (s == "islogical") { out = BuiltinFn::IsLogical; return true; }
    if (s == "iscell")    { out = BuiltinFn::IsCell;    return true; }
    if (s == "isstruct")  { out = BuiltinFn::IsStruct;  return true; }
    if (s == "isreal")    { out = BuiltinFn::IsReal;    return true; }
    if (s == "isnan")     { out = BuiltinFn::IsNan;     return true; }
    if (s == "isinf")     { out = BuiltinFn::IsInf;     return true; }
    if (s == "isfinite")  { out = BuiltinFn::IsFinite;  return true; }
    if (s == "sum")       { out = BuiltinFn::Sum;       return true; }
    if (s == "prod")      { out = BuiltinFn::Prod;      return true; }
    if (s == "mean")      { out = BuiltinFn::Mean;      return true; }
    if (s == "class")     { out = BuiltinFn::ClassName; return true; }
    return false;
}

inline const char *classNameOf(const Value &v)
{
    switch (v.type()) {
    case ValueType::DOUBLE:      return "double";
    case ValueType::SINGLE:      return "single";
    case ValueType::COMPLEX:     return "double";  // MATLAB: complex numbers are class 'double'
    case ValueType::LOGICAL:     return "logical";
    case ValueType::CHAR:        return "char";
    case ValueType::CELL:        return "cell";
    case ValueType::STRUCT:      return "struct";
    case ValueType::FUNC_HANDLE: return "function_handle";
    case ValueType::STRING:      return "string";
    case ValueType::INT8:        return "int8";
    case ValueType::INT16:       return "int16";
    case ValueType::INT32:       return "int32";
    case ValueType::INT64:       return "int64";
    case ValueType::UINT8:       return "uint8";
    case ValueType::UINT16:      return "uint16";
    case ValueType::UINT32:      return "uint32";
    case ValueType::UINT64:      return "uint64";
    case ValueType::EMPTY:       return "double";  // empty default class
    }
    return "unknown";
}

inline Value applyBuiltin(std::pmr::memory_resource *mr, BuiltinFn f, const Value &v,
                           const char *fn)
{
    switch (f) {
    case BuiltinFn::Numel:
        return Value::scalar(static_cast<double>(v.numel()), mr);
    case BuiltinFn::Length:
        return ::numkit::builtin::length(mr, v);
    case BuiltinFn::Ndims:
        return ::numkit::builtin::ndims(mr, v);
    case BuiltinFn::IsEmpty:
        return Value::logicalScalar(v.isEmpty() || v.numel() == 0, mr);
    case BuiltinFn::IsNumeric: {
        const auto t = v.type();
        const bool num = (t == ValueType::DOUBLE || t == ValueType::SINGLE
                       || t == ValueType::COMPLEX || t == ValueType::INT8
                       || t == ValueType::INT16 || t == ValueType::INT32
                       || t == ValueType::INT64 || t == ValueType::UINT8
                       || t == ValueType::UINT16 || t == ValueType::UINT32
                       || t == ValueType::UINT64);
        return Value::logicalScalar(num, mr);
    }
    case BuiltinFn::IsChar:
        return Value::logicalScalar(v.isChar(), mr);
    case BuiltinFn::IsLogical:
        return Value::logicalScalar(v.isLogical(), mr);
    case BuiltinFn::IsCell:
        return Value::logicalScalar(v.isCell(), mr);
    case BuiltinFn::IsStruct:
        return Value::logicalScalar(v.isStruct(), mr);
    case BuiltinFn::IsReal:
        return Value::logicalScalar(!v.isComplex(), mr);
    case BuiltinFn::IsNan: {
        if (!v.isScalar())
            throw Error(std::string(fn) + ": @isnan requires each cell to be scalar in uniform mode",
                         0, 0, fn, "", std::string("m:") + fn + ":notScalar");
        return Value::logicalScalar(std::isnan(v.toScalar()), mr);
    }
    case BuiltinFn::IsInf: {
        if (!v.isScalar())
            throw Error(std::string(fn) + ": @isinf requires each cell to be scalar in uniform mode",
                         0, 0, fn, "", std::string("m:") + fn + ":notScalar");
        return Value::logicalScalar(std::isinf(v.toScalar()), mr);
    }
    case BuiltinFn::IsFinite: {
        if (!v.isScalar())
            throw Error(std::string(fn) + ": @isfinite requires each cell to be scalar in uniform mode",
                         0, 0, fn, "", std::string("m:") + fn + ":notScalar");
        return Value::logicalScalar(std::isfinite(v.toScalar()), mr);
    }
    case BuiltinFn::Sum:  return ::numkit::builtin::sum (mr, v);
    case BuiltinFn::Prod: return ::numkit::builtin::prod(mr, v);
    case BuiltinFn::Mean: return ::numkit::builtin::mean(mr, v);
    case BuiltinFn::ClassName: {
        const char *name = classNameOf(v);
        return Value::fromString(std::string(name), mr);
    }
    }
    throw Error(std::string(fn) + ": internal: unhandled builtin",
                 0, 0, fn, "", std::string("m:") + fn + ":internal");
}

inline Value applyHandle(std::pmr::memory_resource *mr, const Value &handle,
                          BuiltinFn builtinTag, bool isBuiltin,
                          const Value &v, Engine *engine, const char *fn)
{
    if (isBuiltin)
        return applyBuiltin(mr, builtinTag, v, fn);
    if (engine == nullptr)
        throw Error(std::string(fn) + ": custom function handles need an "
                     "Engine — use the engine-aware adapter (callback API).",
                     0, 0, fn, "", std::string("m:") + fn + ":fnUnsupported");
    Value arg = v;
    Span<const Value> args(&arg, 1);
    return engine->callFunctionHandle(handle, args);
}

inline Value packUniform(std::pmr::memory_resource *mr, BuiltinFn f,
                          const Value *results, std::size_t n,
                          const Dims &outDims, const char *fn)
{
    const bool wantLogical = builtinReturnsLogical(f);
    if (builtinReturnsString(f))
        throw Error(std::string(fn) + ": @class output must use UniformOutput=false",
                     0, 0, fn, "", std::string("m:") + fn + ":nonUniform");

    const ValueType outT = wantLogical ? ValueType::LOGICAL : ValueType::DOUBLE;
    const size_t r = outDims.rows();
    const size_t c = outDims.cols();
    const size_t p = outDims.is3D() ? outDims.pages() : 0;
    auto out = (p > 0) ? Value::matrix3d(r, c, p, outT, mr)
                       : Value::matrix(r, c, outT, mr);

    for (size_t i = 0; i < n; ++i) {
        const Value &v = results[i];
        if (!v.isScalar())
            throw Error(std::string(fn) + ": fn returned a non-scalar; pass 'UniformOutput', false",
                         0, 0, fn, "", std::string("m:") + fn + ":notScalar");
        if (wantLogical)
            out.logicalDataMut()[i] = v.toBool() ? 1 : 0;
        else
            out.doubleDataMut()[i]  = v.toScalar();
    }
    return out;
}

inline bool parseUniformOutputFlag(Span<const Value> args, size_t dataArgCount,
                                   const char *fn)
{
    bool uniform = true;
    for (size_t i = dataArgCount; i + 1 < args.size(); i += 2) {
        if (!args[i].isChar() && !args[i].isString())
            throw Error(std::string(fn) + ": expected option name (string)",
                         0, 0, fn, "", std::string("m:") + fn + ":badFlag");
        const std::string key = lowerName(args[i].toString());
        if (key == "uniformoutput") {
            uniform = args[i + 1].toBool();
        } else {
            throw Error(std::string(fn) + ": unsupported option '" + key + "'",
                         0, 0, fn, "", std::string("m:") + fn + ":badFlag");
        }
    }
    if ((args.size() - dataArgCount) % 2 != 0)
        throw Error(std::string(fn) + ": option name without value",
                     0, 0, fn, "", std::string("m:") + fn + ":badFlag");
    return uniform;
}

} // namespace numkit::builtin::detail::handlefn
