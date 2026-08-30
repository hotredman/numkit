// bundle/src/register/builtin/fieldtest_fns_reg.cpp
//
// Four small functions with proven real-world demand from the fieldtest
// corpus: web (no-op in CLI), genpath, vec2ind, rands. Lives in the
// registration layer because it needs Engine (outputText, resolvePath, rng).

#include <numkit/core/engine.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/span.hpp>
#include <numkit/ops/rng_context.hpp>
#include <numkit/fs/vfs.hpp>

#include <cmath>
#include <random>
#include <string>

namespace numkit::builtin::detail {

void web_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty() || (!args[0].isChar() && !args[0].isString()))
        throw Error("web: requires a string URL", 0, 0, "web", "", "numkit:web:nargin");
    ctx.engine->outputText(
        "Warning: web: no browser in the numkit CLI — URL '" + args[0].toString() + "' not opened.\n");
    outs[0] = Value::scalar(0.0);
}

void genpath_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty() || !args[0].isChar())
        throw Error("genpath: requires a string directory name", 0, 0, "genpath", "", "numkit:genpath:nargin");
    auto *mr = ctx.engine->resource();
    std::string root = args[0].toString();
    std::string result = root;
    try {
        auto rp = ctx.engine->resolvePath(root);
        if (rp.fs) {
            auto entries = rp.fs->listDir(rp.path);
            for (const auto &e : entries)
                if (e.isDirectory)
                    result += ";" + root + "/" + e.name;
        }
    } catch (...) { /* lenient — never crash addpath(genpath(...)) */ }
    outs[0] = Value::fromString(result, mr);
}

void vec2ind_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("vec2ind: requires at least 1 argument", 0, 0, "vec2ind", "", "numkit:vec2ind:nargin");
    auto *mr = ctx.engine->resource();
    const Value &v = args[0];
    if (v.dims().rows() == 1) {
        auto out = Value::matrix(1, v.numel(), ValueType::DOUBLE, mr);
        for (size_t i = 0; i < v.numel(); ++i) out.doubleDataMut()[i] = 1.0;
        outs[0] = std::move(out);
        return;
    }
    const size_t rows = v.dims().rows(), cols = v.dims().cols();
    auto out = Value::matrix(1, cols, ValueType::DOUBLE, mr);
    const double *p = v.doubleData();
    for (size_t j = 0; j < cols; ++j) {
        double best = 1.0, bestDist = 2.0;
        for (size_t i = 0; i < rows; ++i) {
            double d = std::abs(p[i + j*rows] - 1.0);
            if (d < bestDist) { bestDist = d; best = static_cast<double>(i+1); }
        }
        out.doubleDataMut()[j] = best;
    }
    outs[0] = std::move(out);
}

void rands_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    size_t m = 1, n = 1;
    if (args.size() >= 2) { m = static_cast<size_t>(args[0].toScalar()); n = static_cast<size_t>(args[1].toScalar()); }
    else if (args.size() == 1) { n = static_cast<size_t>(args[0].toScalar()); }
    auto out = Value::matrix(m, n, ValueType::DOUBLE, mr);
    double *p = out.doubleDataMut();
    auto &rng = ctx.engine->rng();
    std::uniform_real_distribution<double> dist(-1.0, 1.0);
    for (size_t i = 0; i < m * n; ++i) p[i] = dist(rng);
    outs[0] = std::move(out);
}

} // namespace numkit::builtin::detail
