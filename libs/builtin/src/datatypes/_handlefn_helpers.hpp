// libs/builtin/src/datatypes/_handlefn_helpers.hpp
//
// Shared cellfun / structfun helpers — built-in fast-path dispatch for
// common function handles (numel, length, isnumeric, sum, ...) plus the
// callback fallback for anonymous handles. Lifted out of the original
// MStdCellStruct.cpp so cell.cpp and struct.cpp can share it without
// risking ODR violations.
//
// Everything is `inline` to keep this header-only.

#pragma once

#include <numkit/m/builtin/math/elementary/reductions.hpp>   // sum / prod / mean
#include <numkit/m/builtin/lang/arrays/matrix.hpp>  // length / ndims
#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>
#include <numkit/m/core/MValue.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>
#include <vector>

namespace numkit::m::builtin::detail::handlefn {

using ::numkit::m::Engine;

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
inline bool tryParseBuiltinHandle(const MValue &h, BuiltinFn &out, const char *fn)
{
    const MValue *bare = &h;
    if (h.isCell() && h.numel() >= 1 && h.cellAt(0).isFuncHandle())
        bare = &h.cellAt(0);
    if (!bare->isFuncHandle())
        throw MError(std::string(fn) + ": fn argument must be a function handle",
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

inline const char *classNameOf(const MValue &v)
{
    switch (v.type()) {
    case MType::DOUBLE:      return "double";
    case MType::SINGLE:      return "single";
    case MType::COMPLEX:     return "double";  // MATLAB: complex numbers are class 'double'
    case MType::LOGICAL:     return "logical";
    case MType::CHAR:        return "char";
    case MType::CELL:        return "cell";
    case MType::STRUCT:      return "struct";
    case MType::FUNC_HANDLE: return "function_handle";
    case MType::STRING:      return "string";
    case MType::INT8:        return "int8";
    case MType::INT16:       return "int16";
    case MType::INT32:       return "int32";
    case MType::INT64:       return "int64";
    case MType::UINT8:       return "uint8";
    case MType::UINT16:      return "uint16";
    case MType::UINT32:      return "uint32";
    case MType::UINT64:      return "uint64";
    case MType::EMPTY:       return "double";  // empty default class
    }
    return "unknown";
}

inline MValue applyBuiltin(Allocator &alloc, BuiltinFn f, const MValue &v,
                           const char *fn)
{
    switch (f) {
    case BuiltinFn::Numel:
        return MValue::scalar(static_cast<double>(v.numel()), &alloc);
    case BuiltinFn::Length:
        return ::numkit::m::builtin::length(alloc, v);
    case BuiltinFn::Ndims:
        return ::numkit::m::builtin::ndims(alloc, v);
    case BuiltinFn::IsEmpty:
        return MValue::logicalScalar(v.isEmpty() || v.numel() == 0, &alloc);
    case BuiltinFn::IsNumeric: {
        const auto t = v.type();
        const bool num = (t == MType::DOUBLE || t == MType::SINGLE
                       || t == MType::COMPLEX || t == MType::INT8
                       || t == MType::INT16 || t == MType::INT32
                       || t == MType::INT64 || t == MType::UINT8
                       || t == MType::UINT16 || t == MType::UINT32
                       || t == MType::UINT64);
        return MValue::logicalScalar(num, &alloc);
    }
    case BuiltinFn::IsChar:
        return MValue::logicalScalar(v.isChar(), &alloc);
    case BuiltinFn::IsLogical:
        return MValue::logicalScalar(v.isLogical(), &alloc);
    case BuiltinFn::IsCell:
        return MValue::logicalScalar(v.isCell(), &alloc);
    case BuiltinFn::IsStruct:
        return MValue::logicalScalar(v.isStruct(), &alloc);
    case BuiltinFn::IsReal:
        return MValue::logicalScalar(!v.isComplex(), &alloc);
    case BuiltinFn::IsNan: {
        if (!v.isScalar())
            throw MError(std::string(fn) + ": @isnan requires each cell to be scalar in uniform mode",
                         0, 0, fn, "", std::string("m:") + fn + ":notScalar");
        return MValue::logicalScalar(std::isnan(v.toScalar()), &alloc);
    }
    case BuiltinFn::IsInf: {
        if (!v.isScalar())
            throw MError(std::string(fn) + ": @isinf requires each cell to be scalar in uniform mode",
                         0, 0, fn, "", std::string("m:") + fn + ":notScalar");
        return MValue::logicalScalar(std::isinf(v.toScalar()), &alloc);
    }
    case BuiltinFn::IsFinite: {
        if (!v.isScalar())
            throw MError(std::string(fn) + ": @isfinite requires each cell to be scalar in uniform mode",
                         0, 0, fn, "", std::string("m:") + fn + ":notScalar");
        return MValue::logicalScalar(std::isfinite(v.toScalar()), &alloc);
    }
    case BuiltinFn::Sum:  return ::numkit::m::builtin::sum (alloc, v);
    case BuiltinFn::Prod: return ::numkit::m::builtin::prod(alloc, v);
    case BuiltinFn::Mean: return ::numkit::m::builtin::mean(alloc, v);
    case BuiltinFn::ClassName: {
        const char *name = classNameOf(v);
        return MValue::fromString(std::string(name), &alloc);
    }
    }
    throw MError(std::string(fn) + ": internal: unhandled builtin",
                 0, 0, fn, "", std::string("m:") + fn + ":internal");
}

inline MValue applyHandle(Allocator &alloc, const MValue &handle,
                          BuiltinFn builtinTag, bool isBuiltin,
                          const MValue &v, Engine *engine, const char *fn)
{
    if (isBuiltin)
        return applyBuiltin(alloc, builtinTag, v, fn);
    if (engine == nullptr)
        throw MError(std::string(fn) + ": custom function handles need an "
                     "Engine — use the engine-aware adapter (callback API).",
                     0, 0, fn, "", std::string("m:") + fn + ":fnUnsupported");
    MValue arg = v;
    Span<const MValue> args(&arg, 1);
    return engine->callFunctionHandle(handle, args);
}

inline MValue packUniform(Allocator &alloc, BuiltinFn f,
                          const std::vector<MValue> &results,
                          const Dims &outDims, const char *fn)
{
    const bool wantLogical = builtinReturnsLogical(f);
    if (builtinReturnsString(f))
        throw MError(std::string(fn) + ": @class output must use UniformOutput=false",
                     0, 0, fn, "", std::string("m:") + fn + ":nonUniform");

    const MType outT = wantLogical ? MType::LOGICAL : MType::DOUBLE;
    const size_t r = outDims.rows();
    const size_t c = outDims.cols();
    const size_t p = outDims.is3D() ? outDims.pages() : 0;
    auto out = (p > 0) ? MValue::matrix3d(r, c, p, outT, &alloc)
                       : MValue::matrix(r, c, outT, &alloc);

    for (size_t i = 0; i < results.size(); ++i) {
        const MValue &v = results[i];
        if (!v.isScalar())
            throw MError(std::string(fn) + ": fn returned a non-scalar; pass 'UniformOutput', false",
                         0, 0, fn, "", std::string("m:") + fn + ":notScalar");
        if (wantLogical)
            out.logicalDataMut()[i] = v.toBool() ? 1 : 0;
        else
            out.doubleDataMut()[i]  = v.toScalar();
    }
    return out;
}

inline bool parseUniformOutputFlag(Span<const MValue> args, size_t dataArgCount,
                                   const char *fn)
{
    bool uniform = true;
    for (size_t i = dataArgCount; i + 1 < args.size(); i += 2) {
        if (!args[i].isChar() && !args[i].isString())
            throw MError(std::string(fn) + ": expected option name (string)",
                         0, 0, fn, "", std::string("m:") + fn + ":badFlag");
        const std::string key = lowerName(args[i].toString());
        if (key == "uniformoutput") {
            uniform = args[i + 1].toBool();
        } else {
            throw MError(std::string(fn) + ": unsupported option '" + key + "'",
                         0, 0, fn, "", std::string("m:") + fn + ":badFlag");
        }
    }
    if ((args.size() - dataArgCount) % 2 != 0)
        throw MError(std::string(fn) + ": option name without value",
                     0, 0, fn, "", std::string("m:") + fn + ":badFlag");
    return uniform;
}

} // namespace numkit::m::builtin::detail::handlefn
