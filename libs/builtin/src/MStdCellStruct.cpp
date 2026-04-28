// libs/builtin/src/MStdCellStruct.cpp

#include <numkit/m/builtin/MStdCellStruct.hpp>
#include <numkit/m/builtin/MStdLibrary.hpp>
#include <numkit/m/builtin/MStdMath.hpp>
#include <numkit/m/builtin/MStdMatrix.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <string>

namespace numkit::m::builtin {

// ════════════════════════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════════════════════════

MValue structure(Allocator &)
{
    return MValue::structure();
}

MValue structure(Allocator &, Span<const MValue> nameValuePairs)
{
    auto s = MValue::structure();
    for (size_t i = 0; i + 1 < nameValuePairs.size(); i += 2) {
        if (!nameValuePairs[i].isChar() && !nameValuePairs[i].isString())
            throw MError("struct: field names must be char arrays", 0, 0, "struct", "",
                         "m:struct:invalidFieldName");
        s.field(nameValuePairs[i].toString()) = nameValuePairs[i + 1];
    }
    return s;
}

MValue fieldnames(Allocator &alloc, const MValue &s)
{
    if (!s.isStruct())
        throw MError("fieldnames requires a struct", 0, 0, "fieldnames", "",
                     "m:fieldnames:notStruct");
    const auto &fields = s.structFields();
    auto c = MValue::cell(fields.size(), 1);
    size_t i = 0;
    for (const auto &[k, v] : fields)
        c.cellAt(i++) = MValue::fromString(k, &alloc);
    return c;
}

MValue isfield(Allocator &alloc, const MValue &s, const MValue &name)
{
    if (!s.isStruct())
        return MValue::logicalScalar(false, &alloc);
    return MValue::logicalScalar(s.hasField(name.toString()), &alloc);
}

MValue rmfield(Allocator &, const MValue &s, const MValue &name)
{
    if (!s.isStruct())
        throw MError("rmfield requires a struct", 0, 0, "rmfield", "",
                     "m:rmfield:notStruct");
    MValue out = s;
    out.structFields().erase(name.toString());
    return out;
}

MValue cell(Allocator &, size_t n)
{
    return MValue::cell(n, n);
}

MValue cell(Allocator &, size_t rows, size_t cols)
{
    return MValue::cell(rows, cols);
}

MValue cell(Allocator &, size_t rows, size_t cols, size_t pages)
{
    if (pages > 0)
        return MValue::cell3D(rows, cols, pages);
    return MValue::cell(rows, cols);
}

// ── cellfun / structfun helpers ──────────────────────────────────────
namespace {

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

bool builtinReturnsLogical(BuiltinFn f)
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

bool builtinReturnsString(BuiltinFn f)
{
    return f == BuiltinFn::ClassName;
}

std::string lowerName(const std::string &s)
{
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return r;
}

BuiltinFn parseBuiltinHandle(const MValue &h, const char *fn)
{
    if (!h.isFuncHandle())
        throw MError(std::string(fn) + ": fn argument must be a function handle",
                     0, 0, fn, "", std::string("m:") + fn + ":fnType");
    const std::string s = lowerName(h.funcHandleName());
    if (s == "numel")     return BuiltinFn::Numel;
    if (s == "length")    return BuiltinFn::Length;
    if (s == "ndims")     return BuiltinFn::Ndims;
    if (s == "isempty")   return BuiltinFn::IsEmpty;
    if (s == "isnumeric") return BuiltinFn::IsNumeric;
    if (s == "ischar")    return BuiltinFn::IsChar;
    if (s == "islogical") return BuiltinFn::IsLogical;
    if (s == "iscell")    return BuiltinFn::IsCell;
    if (s == "isstruct")  return BuiltinFn::IsStruct;
    if (s == "isreal")    return BuiltinFn::IsReal;
    if (s == "isnan")     return BuiltinFn::IsNan;
    if (s == "isinf")     return BuiltinFn::IsInf;
    if (s == "isfinite")  return BuiltinFn::IsFinite;
    if (s == "sum")       return BuiltinFn::Sum;
    if (s == "prod")      return BuiltinFn::Prod;
    if (s == "mean")      return BuiltinFn::Mean;
    if (s == "class")     return BuiltinFn::ClassName;
    throw MError(std::string(fn) + ": unsupported function handle '@" + s
                 + "' (built-ins: @numel/@length/@ndims/@isempty/"
                 + "@isnumeric/@ischar/@islogical/@iscell/@isstruct/"
                 + "@isreal/@isnan/@isinf/@isfinite/@sum/@prod/@mean/@class). "
                 + "Custom handles need the engine callback API (planned).",
                 0, 0, fn, "", std::string("m:") + fn + ":fnUnsupported");
}

const char *classNameOf(const MValue &v)
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

MValue applyBuiltin(Allocator &alloc, BuiltinFn f, const MValue &v,
                    const char *fn)
{
    switch (f) {
    case BuiltinFn::Numel:
        return MValue::scalar(static_cast<double>(v.numel()), &alloc);
    case BuiltinFn::Length:
        return length(alloc, v);
    case BuiltinFn::Ndims:
        return ndims(alloc, v);
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
        // Per MATLAB: cellfun(@isnan, C) requires each cell to be scalar.
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
    case BuiltinFn::Sum:  return sum (alloc, v);
    case BuiltinFn::Prod: return prod(alloc, v);
    case BuiltinFn::Mean: return mean(alloc, v);
    case BuiltinFn::ClassName: {
        const char *name = classNameOf(v);
        return MValue::fromString(std::string(name), &alloc);
    }
    }
    throw MError(std::string(fn) + ": internal: unhandled builtin",
                 0, 0, fn, "", std::string("m:") + fn + ":internal");
}

// Pack `n` scalar results into a numeric / logical container of `outDims`.
// Throws if any result is not scalar (uniform mode contract).
MValue packUniform(Allocator &alloc, BuiltinFn f,
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

// Parse trailing ('UniformOutput', tf) name-value pair from `args` (after
// the leading fn + data args). `dataArgCount` is how many positional
// arguments precede the flags. Returns the parsed uniformOutput flag.
bool parseUniformOutputFlag(Span<const MValue> args, size_t dataArgCount,
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

} // namespace

MValue cellfun(Allocator &alloc, const MValue &fn, const MValue &c,
               bool uniformOutput)
{
    if (!c.isCell())
        throw MError("cellfun: second argument must be a cell array",
                     0, 0, "cellfun", "", "m:cellfun:notCell");
    BuiltinFn f = parseBuiltinHandle(fn, "cellfun");

    const size_t n = c.numel();
    std::vector<MValue> results;
    results.reserve(n);
    for (size_t i = 0; i < n; ++i)
        results.push_back(applyBuiltin(alloc, f, c.cellAt(i), "cellfun"));

    if (uniformOutput)
        return packUniform(alloc, f, results, c.dims(), "cellfun");

    // Cell output, same shape as C.
    const auto &d = c.dims();
    const size_t r = d.rows();
    const size_t cc = d.cols();
    const size_t p = d.is3D() ? d.pages() : 0;
    MValue out = (p > 0) ? MValue::cell3D(r, cc, p) : MValue::cell(r, cc);
    for (size_t i = 0; i < n; ++i)
        out.cellAt(i) = std::move(results[i]);
    return out;
}

MValue structfun(Allocator &alloc, const MValue &fn, const MValue &s,
                 bool uniformOutput)
{
    if (!s.isStruct())
        throw MError("structfun: second argument must be a scalar struct",
                     0, 0, "structfun", "", "m:structfun:notStruct");
    BuiltinFn f = parseBuiltinHandle(fn, "structfun");

    const auto &fields = s.structFields();
    const size_t n = fields.size();
    std::vector<MValue> results;
    results.reserve(n);
    for (const auto &kv : fields)
        results.push_back(applyBuiltin(alloc, f, kv.second, "structfun"));

    if (uniformOutput) {
        // Uniform: column vector of length n.
        if (builtinReturnsString(f))
            throw MError("structfun: @class output must use UniformOutput=false",
                         0, 0, "structfun", "", "m:structfun:nonUniform");
        const MType outT = builtinReturnsLogical(f) ? MType::LOGICAL : MType::DOUBLE;
        auto out = MValue::matrix(n, 1, outT, &alloc);
        for (size_t i = 0; i < n; ++i) {
            const MValue &v = results[i];
            if (!v.isScalar())
                throw MError("structfun: fn returned a non-scalar; pass 'UniformOutput', false",
                             0, 0, "structfun", "", "m:structfun:notScalar");
            if (outT == MType::LOGICAL)
                out.logicalDataMut()[i] = v.toBool() ? 1 : 0;
            else
                out.doubleDataMut()[i]  = v.toScalar();
        }
        return out;
    }

    // Cell output, column vector (1 entry per field).
    auto out = MValue::cell(n, 1);
    for (size_t i = 0; i < n; ++i)
        out.cellAt(i) = std::move(results[i]);
    return out;
}

// ════════════════════════════════════════════════════════════════════════
// Adapters
// ════════════════════════════════════════════════════════════════════════

namespace detail {

void struct_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    outs[0] = structure(ctx.engine->allocator(), args);
}

void fieldnames_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    if (args.empty())
        throw MError("fieldnames: requires 1 argument", 0, 0, "fieldnames", "",
                     "m:fieldnames:nargin");
    outs[0] = fieldnames(ctx.engine->allocator(), args[0]);
}

void isfield_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("isfield requires 2 arguments", 0, 0, "isfield", "",
                     "m:isfield:nargin");
    outs[0] = isfield(ctx.engine->allocator(), args[0], args[1]);
}

void rmfield_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("rmfield requires 2 arguments", 0, 0, "rmfield", "",
                     "m:rmfield:nargin");
    outs[0] = rmfield(ctx.engine->allocator(), args[0], args[1]);
}

void cellfun_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("cellfun: requires at least 2 arguments (fn, C)",
                     0, 0, "cellfun", "", "m:cellfun:nargin");
    // Multi-cell form throws — single-cell only for tier-1.
    bool uniform = parseUniformOutputFlag(args, 2, "cellfun");
    outs[0] = cellfun(ctx.engine->allocator(), args[0], args[1], uniform);
}

void structfun_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("structfun: requires at least 2 arguments (fn, S)",
                     0, 0, "structfun", "", "m:structfun:nargin");
    bool uniform = parseUniformOutputFlag(args, 2, "structfun");
    outs[0] = structfun(ctx.engine->allocator(), args[0], args[1], uniform);
}

void cell_reg(Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx)
{
    Allocator &alloc = ctx.engine->allocator();
    (void)alloc;
    if (args.empty())
        throw MError("cell: requires 1 argument", 0, 0, "cell", "", "m:cell:nargin");
    // Single vector arg: cell([m n p ...]).
    if (args.size() == 1 && !args[0].isScalar() && args[0].numel() >= 2) {
        const size_t n = args[0].numel();
        std::vector<size_t> dims(n);
        for (size_t i = 0; i < n; ++i)
            dims[i] = static_cast<size_t>(args[0].elemAsDouble(i));
        outs[0] = MValue::cellND(dims.data(), static_cast<int>(n));
        return;
    }
    size_t r = static_cast<size_t>(args[0].toScalar());
    if (args.size() == 1) {
        outs[0] = MValue::cell(r, r);
        return;
    }
    if (args.size() == 2) {
        size_t c = static_cast<size_t>(args[1].toScalar());
        outs[0] = MValue::cell(r, c);
        return;
    }
    if (args.size() == 3) {
        size_t c = static_cast<size_t>(args[1].toScalar());
        size_t p = static_cast<size_t>(args[2].toScalar());
        outs[0] = (p > 0) ? MValue::cell3D(r, c, p) : MValue::cell(r, c);
        return;
    }
    // 4+ scalar args: cell(m, n, p, q, ...).
    std::vector<size_t> dims(args.size());
    for (size_t i = 0; i < args.size(); ++i)
        dims[i] = static_cast<size_t>(args[i].toScalar());
    outs[0] = MValue::cellND(dims.data(), static_cast<int>(args.size()));
}

} // namespace detail

} // namespace numkit::m::builtin

// ════════════════════════════════════════════════════════════════════════
// Registration — keep the registerCellStructFunctions hook empty; actual
// wiring happens in MStdLibrary.cpp via Phase-6c function pointers.
// ════════════════════════════════════════════════════════════════════════

namespace numkit::m {

void StdLibrary::registerCellStructFunctions(Engine &)
{
    // Intentionally empty — see StdLibrary::install() in MStdLibrary.cpp.
}

} // namespace numkit::m
