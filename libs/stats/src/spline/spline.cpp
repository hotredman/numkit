// libs/stats/src/spline/spline.cpp

#include <numkit/stats/spline/spline.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <vector>

namespace numkit::stats {

Value aveknt(std::pmr::memory_resource *mr, const Value &t, int k)
{
    const size_t N = t.numel();
    if (k < 2 || static_cast<size_t>(k) > N)
        throw Error("aveknt: order k must satisfy 2 ≤ k ≤ length(t)",
                    0, 0, "aveknt", "", "m:aveknt:k");
    const size_t M = N - static_cast<size_t>(k);
    Value out = Value::matrix(1, M, ValueType::DOUBLE, mr);
    if (M == 0) return out;
    double *od = out.doubleDataMut();
    const double inv = 1.0 / static_cast<double>(k - 1);
    for (size_t i = 0; i < M; ++i) {
        double sum = 0.0;
        for (size_t j = 1; j < static_cast<size_t>(k); ++j)
            sum += t.elemAsDouble(i + j);
        od[i] = sum * inv;
    }
    return out;
}

Value augknt(std::pmr::memory_resource *mr, const Value &knots, int k)
{
    const size_t N = knots.numel();
    if (k < 1 || N == 0)
        throw Error("augknt: k must be ≥ 1 and knots non-empty",
                    0, 0, "augknt", "", "m:augknt:k");
    // The first knot becomes k copies; the last knot becomes k copies;
    // any interior knots stay single. So size = N + 2*(k-1).
    const size_t outN = N + 2 * (static_cast<size_t>(k) - 1);
    Value out = Value::matrix(1, outN, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    const double first = knots.elemAsDouble(0);
    const double last  = knots.elemAsDouble(N - 1);
    size_t idx = 0;
    for (int i = 0; i < k; ++i) od[idx++] = first;
    for (size_t i = 1; i + 1 < N; ++i) od[idx++] = knots.elemAsDouble(i);
    for (int i = 0; i < k; ++i) od[idx++] = last;
    return out;
}

Value brk2knt(std::pmr::memory_resource *mr, const Value &breaks, const Value &mults)
{
    const size_t N = breaks.numel();
    if (mults.numel() != N)
        throw Error("brk2knt: breaks and mults must have same length",
                    0, 0, "brk2knt", "", "m:brk2knt:size");
    size_t total = 0;
    for (size_t i = 0; i < N; ++i) {
        const double mi = mults.elemAsDouble(i);
        if (!(mi >= 0.0))
            throw Error("brk2knt: multiplicities must be non-negative",
                        0, 0, "brk2knt", "", "m:brk2knt:m");
        total += static_cast<size_t>(mi);
    }
    Value out = Value::matrix(1, total, ValueType::DOUBLE, mr);
    if (total == 0) return out;
    double *od = out.doubleDataMut();
    size_t idx = 0;
    for (size_t i = 0; i < N; ++i) {
        const size_t mi = static_cast<size_t>(mults.elemAsDouble(i));
        const double bi = breaks.elemAsDouble(i);
        for (size_t j = 0; j < mi; ++j) od[idx++] = bi;
    }
    return out;
}

std::tuple<Value, Value>
knt2brk(std::pmr::memory_resource *mr, const Value &knots)
{
    const size_t N = knots.numel();
    std::vector<double> b;
    std::vector<size_t> m;
    if (N == 0) {
        Value bv = Value::matrix(0, 0, ValueType::DOUBLE, mr);
        Value mv = Value::matrix(0, 0, ValueType::DOUBLE, mr);
        return {std::move(bv), std::move(mv)};
    }
    double cur = knots.elemAsDouble(0);
    size_t cnt = 1;
    for (size_t i = 1; i < N; ++i) {
        const double ki = knots.elemAsDouble(i);
        if (ki == cur) {
            ++cnt;
        } else {
            b.push_back(cur);
            m.push_back(cnt);
            cur = ki;
            cnt = 1;
        }
    }
    b.push_back(cur);
    m.push_back(cnt);

    const size_t K = b.size();
    Value bv = Value::matrix(1, K, ValueType::DOUBLE, mr);
    Value mv = Value::matrix(1, K, ValueType::DOUBLE, mr);
    double *bd = bv.doubleDataMut();
    double *md = mv.doubleDataMut();
    for (size_t i = 0; i < K; ++i) { bd[i] = b[i]; md[i] = double(m[i]); }
    return {std::move(bv), std::move(mv)};
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

void aveknt_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("aveknt: requires (t, k)",
                    0, 0, "aveknt", "", "m:aveknt:nargin");
    outs[0] = aveknt(ctx.engine->resource(), args[0],
                     static_cast<int>(args[1].toScalar()));
}

void augknt_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("augknt: requires (knots, k)",
                    0, 0, "augknt", "", "m:augknt:nargin");
    outs[0] = augknt(ctx.engine->resource(), args[0],
                     static_cast<int>(args[1].toScalar()));
}

void brk2knt_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("brk2knt: requires (breaks, mults)",
                    0, 0, "brk2knt", "", "m:brk2knt:nargin");
    outs[0] = brk2knt(ctx.engine->resource(), args[0], args[1]);
}

void knt2brk_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("knt2brk: requires (knots)",
                    0, 0, "knt2brk", "", "m:knt2brk:nargin");
    auto [b, m] = knt2brk(ctx.engine->resource(), args[0]);
    outs[0] = std::move(b);
    if (nargout > 1) outs[1] = std::move(m);
}

} // namespace detail
} // namespace numkit::stats
