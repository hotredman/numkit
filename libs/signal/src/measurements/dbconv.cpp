// libs/signal/src/measurements/dbconv.cpp
//
// db / db2mag / mag2db / pow2db / db2pow.

#include <numkit/signal/measurements/dbconv.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"  // createLike (libs/builtin/src/)

#include <algorithm>
#include <cctype>
#include <cmath>
#include <complex>
#include <string>

namespace numkit::signal {

namespace {

// Walk every element of x, taking magnitude when complex; pass to f.
template <typename F>
void forEachAsMag(const Value &x, double *dst, F &&f)
{
    const size_t n = x.numel();
    if (x.isComplex()) {
        const Complex *src = x.complexData();
        for (size_t i = 0; i < n; ++i)
            dst[i] = f(std::abs(src[i]));
    } else {
        const double *src = x.doubleData();
        for (size_t i = 0; i < n; ++i)
            dst[i] = f(src[i]);
    }
}

std::string toLower(const std::string &s)
{
    std::string r = s;
    for (auto &c : r) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return r;
}

} // namespace

// ── db ─────────────────────────────────────────────────────────────────
Value db(std::pmr::memory_resource *mr, const Value &x, const std::string &signalType)
{
    const std::string mode = toLower(signalType);
    double scale;
    if (mode == "voltage" || mode.empty())
        scale = 20.0;
    else if (mode == "power")
        scale = 10.0;
    else
        throw Error("db: signalType must be 'voltage' or 'power'",
                     0, 0, "db", "", "m:db:badType");
    auto out = createLike(x, ValueType::DOUBLE, mr);
    forEachAsMag(x, out.doubleDataMut(),
                 [scale](double v) { return scale * std::log10(v); });
    return out;
}

// ── db2mag ─────────────────────────────────────────────────────────────
Value db2mag(std::pmr::memory_resource *mr, const Value &d)
{
    auto out = createLike(d, ValueType::DOUBLE, mr);
    forEachAsMag(d, out.doubleDataMut(),
                 [](double v) { return std::pow(10.0, v / 20.0); });
    return out;
}

// ── mag2db ─────────────────────────────────────────────────────────────
Value mag2db(std::pmr::memory_resource *mr, const Value &x)
{
    auto out = createLike(x, ValueType::DOUBLE, mr);
    forEachAsMag(x, out.doubleDataMut(),
                 [](double v) { return 20.0 * std::log10(v); });
    return out;
}

// ── db2pow ─────────────────────────────────────────────────────────────
Value db2pow(std::pmr::memory_resource *mr, const Value &d)
{
    auto out = createLike(d, ValueType::DOUBLE, mr);
    forEachAsMag(d, out.doubleDataMut(),
                 [](double v) { return std::pow(10.0, v / 10.0); });
    return out;
}

// ── pow2db ─────────────────────────────────────────────────────────────
Value pow2db(std::pmr::memory_resource *mr, const Value &p)
{
    auto out = createLike(p, ValueType::DOUBLE, mr);
    forEachAsMag(p, out.doubleDataMut(),
                 [](double v) { return 10.0 * std::log10(v); });
    return out;
}

namespace detail {

void db_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("db: requires at least 1 argument",
                     0, 0, "db", "", "m:db:nargin");
    std::string mode = "voltage";
    if (args.size() >= 2) {
        if (!args[1].isChar() && !args[1].isString())
            throw Error("db: 2nd argument must be a string",
                         0, 0, "db", "", "m:db:badType");
        mode = args[1].toString();
    }
    outs[0] = db(ctx.engine->resource(), args[0], mode);
}

void db2mag_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("db2mag: requires 1 argument",
                     0, 0, "db2mag", "", "m:db2mag:nargin");
    outs[0] = db2mag(ctx.engine->resource(), args[0]);
}

void mag2db_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("mag2db: requires 1 argument",
                     0, 0, "mag2db", "", "m:mag2db:nargin");
    outs[0] = mag2db(ctx.engine->resource(), args[0]);
}

void db2pow_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("db2pow: requires 1 argument",
                     0, 0, "db2pow", "", "m:db2pow:nargin");
    outs[0] = db2pow(ctx.engine->resource(), args[0]);
}

void pow2db_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("pow2db: requires 1 argument",
                     0, 0, "pow2db", "", "m:pow2db:nargin");
    outs[0] = pow2db(ctx.engine->resource(), args[0]);
}

} // namespace detail

} // namespace numkit::signal
