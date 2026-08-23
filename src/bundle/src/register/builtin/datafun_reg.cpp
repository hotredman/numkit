#include <numkit/builtin/datafun.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/value_type.hpp>
#include <numkit/value/span.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/error.hpp>
#include <numkit/ops/helpers.hpp>

#include <algorithm>
#include <cmath>
#include <cctype>
#include <stdexcept>
#include <string>

namespace numkit::builtin::detail {

void allunique_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void bitand_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void bitcmp_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void bitget_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void bitor_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void bitset_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void bitshift_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void bitxor_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void boundary_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void colperm_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void convhull_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void deg2rad_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void del2_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void delaunay_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void discretize_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void expm1_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void findgroups_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void gradient_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void griddata_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void griddatan_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void groupcounts_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void groupfilter_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void groupsummary_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void grouptransform_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void histc_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void histcounts_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void histcounts2_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void inpolygon_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void intersect_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ismember_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void ismembertol_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void linspace_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void log1p_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void logspace_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void matchpairs_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void max_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void mean_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void min_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void nthroot_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void numunique_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void polyarea_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void pow2_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void prod_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void rad2deg_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void rand_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void randi_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void randn_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void randperm_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void reallog_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void realpow_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void realsqrt_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void rng_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void setdiff_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void setxor_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void splitapply_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void sum_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void symrcm_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void union_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void unique_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void uniquetol_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void wrapTo180_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void wrapTo2Pi_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void wrapTo360_reg(Span<const Value>, size_t, Span<Value>, CallContext&);
void wrapToPi_reg(Span<const Value>, size_t, Span<Value>, CallContext&);

namespace {

bool tryParseReducer(const Value &h, AccumReducer &op)
{
    if (!h.isFuncHandle())
        throw Error("accumarray: fn argument must be a function handle",
                     0, 0, "accumarray", "", "numkit:accumarray:fnType");
    std::string s = h.funcHandleName();
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (s == "sum")  { op = AccumReducer::Sum;  return true; }
    if (s == "max")  { op = AccumReducer::Max;  return true; }
    if (s == "min")  { op = AccumReducer::Min;  return true; }
    if (s == "prod") { op = AccumReducer::Prod; return true; }
    if (s == "mean") { op = AccumReducer::Mean; return true; }
    if (s == "any")  { op = AccumReducer::Any;  return true; }
    if (s == "all")  { op = AccumReducer::All;  return true; }
    return false;
}

size_t toSubIndex(double v, size_t maxAllowed, const char *fn)
{
    if (!std::isfinite(v) || v < 1.0)
        throw Error(std::string(fn) + ": subscripts must be positive integers",
                     0, 0, fn, "", std::string("numkit:") + fn + ":subRange");
    const double rounded = std::round(v);
    if (std::abs(v - rounded) > 1e-9)
        throw Error(std::string(fn) + ": subscripts must be integer-valued",
                     0, 0, fn, "", std::string("numkit:") + fn + ":subInt");
    const size_t idx = static_cast<size_t>(rounded);
    if (maxAllowed > 0 && idx > maxAllowed)
        throw Error(std::string(fn) + ": subscript exceeds output dimension",
                     0, 0, fn, "", std::string("numkit:") + fn + ":subOOB");
    return idx;
}

ScratchVec<size_t> resolveOutShape(const Value &subs, const size_t *userShape,
                                   std::size_t nUserShape, const char *fn,
                                   std::pmr::memory_resource *mr)
{
    const auto &d = subs.dims();
    const size_t N = d.rows();
    const size_t D = (d.ndim() <= 1) ? 1 : d.cols();
    if (nUserShape > 0) {
        if (nUserShape < D)
            throw Error(std::string(fn) + ": sz length must be at least size(subs, 2)",
                         0, 0, fn, "", std::string("numkit:") + fn + ":sizeRank");
        ScratchVec<size_t> out(userShape, userShape + nUserShape, mr);
        return out;
    }
    ScratchVec<size_t> shape(D, mr);
    if (N == 0) {
        return ScratchVec<size_t>(std::max<size_t>(D, 1), 0, mr);
    }
    const double *p = subs.doubleData();
    for (size_t c = 0; c < D; ++c) {
        double mx = 0.0;
        for (size_t r = 0; r < N; ++r) {
            const double v = p[c * N + r];
            if (v > mx) mx = v;
        }
        shape[c] = toSubIndex(mx, 0, fn);
    }
    return shape;
}

size_t linearIndexFromSubs(const Value &subs, size_t r, size_t N, size_t D,
                           const size_t *shape, const char *fn)
{
    const double *p = subs.doubleData();
    size_t idx = 0;
    size_t stride = 1;
    for (size_t c = 0; c < D; ++c) {
        const size_t s = toSubIndex(p[c * N + r], shape[c], fn);
        idx += (s - 1) * stride;
        stride *= shape[c];
    }
    return idx;
}

Value allocOutput(const size_t *shape, std::size_t nShape, std::pmr::memory_resource *mr)
{
    if (nShape == 1)
        return Value::matrix(shape[0], 1, ValueType::DOUBLE, mr);
    if (nShape == 2)
        return Value::matrix(shape[0], shape[1], ValueType::DOUBLE, mr);
    return Value::matrixND(shape, static_cast<int>(nShape),
                            ValueType::DOUBLE, mr);
}

inline double readVal(const Value &vals, size_t i, bool valIsScalar)
{
    return valIsScalar ? vals.toScalar() : vals.doubleData()[i];
}

Value accumarrayGeneral(const Value &subs, const Value &vals,
                        Span<const size_t> outShape, const Value &handle,
                        double fillVal, CallContext &ctx,
                        std::pmr::memory_resource *mr)
{
    const char *fn = "accumarray";
    if (subs.type() != ValueType::DOUBLE)
        throw Error("accumarray: subs must be DOUBLE", 0, 0, fn, "", "numkit:accumarray:subType");
    if (vals.type() != ValueType::DOUBLE)
        throw Error("accumarray: vals must be DOUBLE", 0, 0, fn, "", "numkit:accumarray:valType");
    const auto &sd = subs.dims();
    if (sd.ndim() > 2)
        throw Error("accumarray: subs must be a 2D matrix", 0, 0, fn, "", "numkit:accumarray:subND");
    const size_t N = sd.rows();
    const size_t D = (sd.ndim() <= 1 || sd.cols() == 0) ? 1 : sd.cols();
    const bool valIsScalar = vals.isScalar();
    if (!valIsScalar && vals.numel() != N)
        throw Error("accumarray: vals must be a scalar or a length-N vector",
                     0, 0, fn, "", "numkit:accumarray:valSize");

    ScratchArena scratch(mr);
    auto shape = resolveOutShape(subs, outShape.data(), outShape.size(), fn, &scratch);
    if (shape.size() < D) shape.resize(D, 1);
    Value out = allocOutput(shape.data(), shape.size(), mr);
    const size_t total = out.numel();
    double *dst = out.doubleDataMut();
    if (total == 0) return out;

    auto lins = ScratchVec<size_t>(N, &scratch);
    auto cnt  = ScratchVec<size_t>(total, 0, &scratch);
    for (size_t r = 0; r < N; ++r) {
        lins[r] = linearIndexFromSubs(subs, r, N, D, shape.data(), fn);
        ++cnt[lins[r]];
    }
    auto off = ScratchVec<size_t>(total + 1, 0, &scratch);
    for (size_t i = 0; i < total; ++i) off[i + 1] = off[i] + cnt[i];
    auto ordered = ScratchVec<double>(N, &scratch);
    auto cur = ScratchVec<size_t>(total, 0, &scratch);
    for (size_t r = 0; r < N; ++r) {
        const size_t c = lins[r];
        ordered[off[c] + cur[c]++] = readVal(vals, r, valIsScalar);
    }

    for (size_t i = 0; i < total; ++i) {
        if (cnt[i] == 0) { dst[i] = fillVal; continue; }
        const size_t len = cnt[i];
        Value g = Value::matrix(len, 1, ValueType::DOUBLE, mr);
        std::copy(ordered.begin() + off[i], ordered.begin() + off[i] + len,
                  g.doubleDataMut());
        Value callArgs[1] = { std::move(g) };
        Value res = ctx.engine->callFunctionHandle(handle,
                                                   Span<const Value>(callArgs, 1));
        dst[i] = res.toScalar();
    }
    return out;
}

ScratchVec<size_t> parseSizeArg(const Value &sz, std::pmr::memory_resource *mr)
{
    ScratchVec<size_t> shape(mr);
    if (sz.isEmpty()) return shape;
    if (sz.type() != ValueType::DOUBLE || !sz.dims().isVector())
        throw Error("accumarray: sz must be a numeric row vector",
                     0, 0, "accumarray", "", "numkit:accumarray:sizeType");
    const size_t k = sz.numel();
    shape.resize(k);
    const double *p = sz.doubleData();
    for (size_t i = 0; i < k; ++i) {
        const double v = p[i];
        if (!std::isfinite(v) || v < 0)
            throw Error("accumarray: sz entries must be non-negative integers",
                         0, 0, "accumarray", "", "numkit:accumarray:sizeRange");
        shape[i] = static_cast<size_t>(std::round(v));
    }
    return shape;
}

} // namespace

void accumarray_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("accumarray: requires at least 2 arguments (subs, vals)",
                     0, 0, "accumarray", "", "numkit:accumarray:nargin");
    if (args.size() > 6)
        throw Error("accumarray: too many arguments",
                     0, 0, "accumarray", "", "numkit:accumarray:nargin");

    auto *mr = ctx.engine->resource();
    ScratchArena scratch(mr);
    auto shape = ScratchVec<size_t>(&scratch);
    if (args.size() >= 3 && !args[2].isEmpty())
        shape = parseSizeArg(args[2], &scratch);

    AccumReducer op = AccumReducer::Sum;
    const Value *customFn = nullptr;
    if (args.size() >= 4 && !args[3].isEmpty()) {
        if (!tryParseReducer(args[3], op)) customFn = &args[3];
    }

    double fillVal = 0.0;
    if (args.size() >= 5 && !args[4].isEmpty()) {
        if (!args[4].isScalar())
            throw Error("accumarray: fillval must be a scalar",
                         0, 0, "accumarray", "", "numkit:accumarray:fillType");
        fillVal = args[4].toScalar();
    }
    if (args.size() >= 6 && !args[5].isEmpty()) {
        if (args[5].toScalar() != 0.0)
            throw Error("accumarray: sparse output (issparse=1) is not supported",
                         0, 0, "accumarray", "", "numkit:accumarray:sparse");
    }

    const Value &valsArg = args[1];
    const bool valsInt = !valsArg.isComplex() && isIntegerType(valsArg.type());
    const bool valsProm = valsInt || valsArg.isLogical();
    Value valsHold;
    if (valsProm) valsHold = toDoubleValue(valsArg, mr);
    const Value &vals = valsProm ? valsHold : valsArg;

    Span<const size_t> shapeSpan(shape.data(), shape.size());
    if (customFn) {
        outs[0] = accumarrayGeneral(args[0], vals, shapeSpan, *customFn, fillVal, ctx, mr);
    } else {
        Value res = accumarray(args[0], vals, shapeSpan, op, fillVal, mr);
        if (valsInt && (op == AccumReducer::Max || op == AccumReducer::Min))
            res = doubleToIntegerExact(res, valsArg.type(), mr);
        outs[0] = std::move(res);
    }
}

} // namespace numkit::builtin::detail

namespace numkit::bundle::builtin {

void register_datafun(Engine &engine) {
    engine.registerFunction("linspace", &::numkit::builtin::detail::linspace_reg);
    engine.registerFunction("logspace", &::numkit::builtin::detail::logspace_reg);
    engine.registerFunction("rand",     &::numkit::builtin::detail::rand_reg);
    engine.registerFunction("randn",    &::numkit::builtin::detail::randn_reg);
    engine.registerFunction("randi",    &::numkit::builtin::detail::randi_reg);
    engine.registerFunction("randperm", &::numkit::builtin::detail::randperm_reg);
    engine.registerFunction("rng",      &::numkit::builtin::detail::rng_reg);

    engine.registerFunction("max",      &::numkit::builtin::detail::max_reg);
    engine.registerFunction("min",      &::numkit::builtin::detail::min_reg);
    engine.registerFunction("sum",      &::numkit::builtin::detail::sum_reg);
    engine.registerFunction("prod",     &::numkit::builtin::detail::prod_reg);
    engine.registerFunction("mean",     &::numkit::builtin::detail::mean_reg);

    engine.registerFunction("nthroot",  &::numkit::builtin::detail::nthroot_reg);
    engine.registerFunction("expm1",    &::numkit::builtin::detail::expm1_reg);
    engine.registerFunction("log1p",    &::numkit::builtin::detail::log1p_reg);
    engine.registerFunction("pow2",     &::numkit::builtin::detail::pow2_reg);
    engine.registerFunction("realpow",  &::numkit::builtin::detail::realpow_reg);
    engine.registerFunction("reallog",  &::numkit::builtin::detail::reallog_reg);
    engine.registerFunction("realsqrt", &::numkit::builtin::detail::realsqrt_reg);

    engine.registerFunction("bitand",   &::numkit::builtin::detail::bitand_reg);
    engine.registerFunction("bitor",    &::numkit::builtin::detail::bitor_reg);
    engine.registerFunction("bitxor",   &::numkit::builtin::detail::bitxor_reg);
    engine.registerFunction("bitshift", &::numkit::builtin::detail::bitshift_reg);
    engine.registerFunction("bitcmp",   &::numkit::builtin::detail::bitcmp_reg);
    engine.registerFunction("bitset",   &::numkit::builtin::detail::bitset_reg);
    engine.registerFunction("bitget",   &::numkit::builtin::detail::bitget_reg);

    engine.registerFunction("unique",     &::numkit::builtin::detail::unique_reg);
    engine.registerFunction("ismember",   &::numkit::builtin::detail::ismember_reg);
    engine.registerFunction("union",      &::numkit::builtin::detail::union_reg);
    engine.registerFunction("intersect",  &::numkit::builtin::detail::intersect_reg);
    engine.registerFunction("setdiff",    &::numkit::builtin::detail::setdiff_reg);
    engine.registerFunction("setxor",     &::numkit::builtin::detail::setxor_reg);
    engine.registerFunction("allunique",  &::numkit::builtin::detail::allunique_reg);
    engine.registerFunction("numunique",  &::numkit::builtin::detail::numunique_reg);
    engine.registerFunction("ismembertol",&::numkit::builtin::detail::ismembertol_reg);
    engine.registerFunction("uniquetol",  &::numkit::builtin::detail::uniquetol_reg);
    engine.registerFunction("histcounts", &::numkit::builtin::detail::histcounts_reg);
    engine.registerFunction("histc",      &::numkit::builtin::detail::histc_reg);
    engine.registerFunction("discretize", &::numkit::builtin::detail::discretize_reg);
    engine.registerFunction("accumarray", &::numkit::builtin::detail::accumarray_reg);
    engine.registerFunction("deg2rad",    &::numkit::builtin::detail::deg2rad_reg);
    engine.registerFunction("rad2deg",    &::numkit::builtin::detail::rad2deg_reg);
    engine.registerFunction("wrapToPi",   &::numkit::builtin::detail::wrapToPi_reg);
    engine.registerFunction("wrapTo2Pi",  &::numkit::builtin::detail::wrapTo2Pi_reg);
    engine.registerFunction("wrapTo180",  &::numkit::builtin::detail::wrapTo180_reg);
    engine.registerFunction("wrapTo360",  &::numkit::builtin::detail::wrapTo360_reg);

    engine.registerFunction("gradient",   &::numkit::builtin::detail::gradient_reg);
    engine.registerFunction("del2",       &::numkit::builtin::detail::del2_reg);
    engine.registerFunction("inpolygon",  &::numkit::builtin::detail::inpolygon_reg);
    engine.registerFunction("convhull",   &::numkit::builtin::detail::convhull_reg);
    engine.registerFunction("polyarea",   &::numkit::builtin::detail::polyarea_reg);
    engine.registerFunction("boundary",   &::numkit::builtin::detail::boundary_reg);
    engine.registerFunction("delaunay",   &::numkit::builtin::detail::delaunay_reg);
    engine.registerFunction("histcounts2",&::numkit::builtin::detail::histcounts2_reg);
    engine.registerFunction("griddata",   &::numkit::builtin::detail::griddata_reg);
    engine.registerFunction("griddatan",  &::numkit::builtin::detail::griddatan_reg);
    engine.registerFunction("matchpairs", &::numkit::builtin::detail::matchpairs_reg);
    engine.registerFunction("findgroups", &::numkit::builtin::detail::findgroups_reg);
    engine.registerFunction("splitapply", &::numkit::builtin::detail::splitapply_reg);
    engine.registerFunction("groupcounts",&::numkit::builtin::detail::groupcounts_reg);
    engine.registerFunction("groupsummary",&::numkit::builtin::detail::groupsummary_reg);
    engine.registerFunction("grouptransform",&::numkit::builtin::detail::grouptransform_reg);
    engine.registerFunction("groupfilter",&::numkit::builtin::detail::groupfilter_reg);
    engine.registerFunction("colperm",    &::numkit::builtin::detail::colperm_reg);
    engine.registerFunction("symrcm",     &::numkit::builtin::detail::symrcm_reg);

    engine.registerFunction("full",
        [](Span<const Value> args, size_t, Span<Value> outs, CallContext &) {
            if (args.empty())
                throw std::runtime_error("full requires 1 argument");
            outs[0] = args[0];
        });
}

} // namespace numkit::bundle::builtin
