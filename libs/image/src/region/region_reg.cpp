// libs/image/src/region/region_reg.cpp
//
// Register half of the image region builtins: the CallContext wrappers
// delegating to the engine-free compute in region.cpp. library.cpp
// forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/image/region/region.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace numkit::image {

namespace detail {

namespace {

// Two-pass chamfer distance transform for non-Euclidean grid metrics.
// dOrth = orthogonal step cost, dDiag = diagonal step cost; allowDiag=false
// forbids diagonal moves (cityblock). Exact for cityblock (dOrth=1, no
// diagonal), chessboard (1,1) and quasi-euclidean (1, sqrt2).
Value bwdistChamfer(const Value &BW, double dOrth, double dDiag,
                    bool allowDiag, std::pmr::memory_resource *mr)
{
    const size_t H = BW.dims().rows();
    const size_t W = BW.dims().cols();
    Value out = Value::matrix(H, W, ValueType::DOUBLE, mr);
    if (H == 0 || W == 0) return out;
    const double INF = std::numeric_limits<double>::infinity();

    std::vector<double> d(H * W, INF);          // row-major working buffer
    for (size_t r = 0; r < H; ++r)
        for (size_t c = 0; c < W; ++c)
            if (BW.elemAsDouble(c * H + r) != 0.0) d[r * W + c] = 0.0;

    auto at = [&](size_t r, size_t c) -> double & { return d[r * W + c]; };

    for (size_t r = 0; r < H; ++r)              // forward raster scan
        for (size_t c = 0; c < W; ++c) {
            double v = at(r, c);
            if (r > 0)                            v = std::min(v, at(r - 1, c) + dOrth);
            if (c > 0)                            v = std::min(v, at(r, c - 1) + dOrth);
            if (allowDiag && r > 0 && c > 0)      v = std::min(v, at(r - 1, c - 1) + dDiag);
            if (allowDiag && r > 0 && c + 1 < W)  v = std::min(v, at(r - 1, c + 1) + dDiag);
            at(r, c) = v;
        }
    for (size_t r = H; r-- > 0; )                // backward raster scan
        for (size_t c = W; c-- > 0; ) {
            double v = at(r, c);
            if (r + 1 < H)                            v = std::min(v, at(r + 1, c) + dOrth);
            if (c + 1 < W)                            v = std::min(v, at(r, c + 1) + dOrth);
            if (allowDiag && r + 1 < H && c + 1 < W)  v = std::min(v, at(r + 1, c + 1) + dDiag);
            if (allowDiag && r + 1 < H && c > 0)      v = std::min(v, at(r + 1, c - 1) + dDiag);
            at(r, c) = v;
        }

    double *od = out.doubleDataMut();
    for (size_t r = 0; r < H; ++r)
        for (size_t c = 0; c < W; ++c)
            od[c * H + r] = d[r * W + c];        // back to column-major
    return out;
}

} // anonymous

void bwlabel_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("bwlabel: requires (BW[, conn])", 0, 0, "bwlabel", "",
                    "numkit:bwlabel:nargin");
    const int conn = (args.size() >= 2 && !args[1].isEmpty())
                     ? (int)args[1].toScalar() : 8;
    auto [L, n] = bwlabel(args[0], conn, ctx.engine->resource());
    outs[0] = std::move(L);
    if (nargout > 1) outs[1] = std::move(n);
}

void bwconncomp_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("bwconncomp: requires (BW[, conn])", 0, 0, "bwconncomp", "",
                    "numkit:bwconncomp:nargin");
    const int conn = (args.size() >= 2 && !args[1].isEmpty())
                     ? (int)args[1].toScalar() : 8;
    outs[0] = bwconncomp(args[0], conn, ctx.engine->resource());
}

void labelmatrix_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("labelmatrix: requires (CC)", 0, 0, "labelmatrix", "",
                    "numkit:labelmatrix:nargin");
    outs[0] = labelmatrix(args[0], ctx.engine->resource());
}

void cc2bw_reg(Span<const Value> args, size_t /*nargout*/,
               Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("cc2bw: requires (CC [, NV...])", 0, 0, "cc2bw", "",
                    "numkit:cc2bw:nargin");
    auto *mr = ctx.engine->resource();
    auto is_string = [](const Value &v) { return v.isChar() || v.isString(); };
    Value objs;  // empty → keep all
    std::size_t i = 1;
    while (i + 1 < args.size()) {
        if (!is_string(args[i]))
            throw Error("cc2bw: expected NV-pair name string",
                        0, 0, "cc2bw", "", "numkit:cc2bw:badNv");
        std::string name = args[i].toString();
        std::string nlo;
        for (char ch : name)
            nlo += static_cast<char>(std::tolower(
                static_cast<unsigned char>(ch)));
        if (nlo == "objectstokeep") {
            objs = args[i + 1];
        } else {
            throw Error("cc2bw: unknown option '" + name + "'",
                        0, 0, "cc2bw", "",
                        "numkit:cc2bw:unknownNv");
        }
        i += 2;
    }
    outs[0] = cc2bw(args[0], objs, mr);
}

void bwarea_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("bwarea: requires BW", 0, 0, "bwarea", "",
                    "numkit:bwarea:nargin");
    outs[0] = bwarea(args[0], ctx.engine->resource());
}

void bwperim_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("bwperim: requires (BW[, conn])", 0, 0, "bwperim", "",
                    "numkit:bwperim:nargin");
    const int conn = (args.size() >= 2 && !args[1].isEmpty())
                     ? (int)args[1].toScalar() : 8;
    outs[0] = bwperim(args[0], conn, ctx.engine->resource());
}

void bwareaopen_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("bwareaopen: requires (BW, P[, conn])", 0, 0,
                    "bwareaopen", "", "numkit:bwareaopen:nargin");
    const int P = (int)args[1].toScalar();
    const int conn = (args.size() >= 3 && !args[2].isEmpty())
                     ? (int)args[2].toScalar() : 8;
    outs[0] = bwareaopen(args[0], P, conn, ctx.engine->resource());
}

void bwboundaries_reg(Span<const Value> args, size_t nargout,
                      Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("bwboundaries: requires (BW[, conn])", 0, 0,
                    "bwboundaries", "", "numkit:bwboundaries:nargin");
    // conn is the first numeric trailing arg; a string (e.g. 'noholes') is a
    // mode flag (numkit traces objects only either way) and is ignored.
    int conn = 8;
    if (args.size() >= 2 && !args[1].isEmpty() && !args[1].isChar() &&
        !args[1].isString())
        conn = (int)args[1].toScalar();
    auto *mr = ctx.engine->resource();
    Value L; int N = 0;
    outs[0] = bwboundaries(args[0], conn,
                           nargout > 1 ? &L : nullptr,
                           nargout > 2 ? &N : nullptr, mr);
    if (nargout > 1) outs[1] = std::move(L);
    if (nargout > 2) outs[2] = Value::scalar(static_cast<double>(N), mr);
}

void regionprops_reg(Span<const Value> args, size_t /*nargout*/,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("regionprops: requires (BW_or_L[, I][, props...])",
                    0, 0, "regionprops", "", "numkit:regionprops:nargin");
    // MATLAB: regionprops(BW_or_L, I, props) — a numeric 2nd argument is the
    // grayscale intensity image for the *Intensity / WeightedCentroid /
    // PixelValues measurements. A string/cell 2nd argument is a property.
    Value intensity = Value();
    size_t propStart = 1;
    if (args.size() >= 2 && !args[1].isChar() && !args[1].isString() &&
        !args[1].isCell()) {
        intensity = args[1];
        propStart = 2;
    }
    std::vector<std::string> props;
    for (size_t i = propStart; i < args.size(); ++i) {
        if (!args[i].isChar() && !args[i].isString())
            throw Error("regionprops: property names must be strings",
                        0, 0, "regionprops", "", "numkit:regionprops:type");
        props.push_back(args[i].toString());
    }
    outs[0] = regionprops(args[0], props, intensity, ctx.engine->resource());
}

// bwdist 2nd output IDX (feature transform): for each pixel, the column-major
// 1-based linear index of the NEAREST foreground pixel, ties broken to the
// lowest linear index (matches MATLAB R2025b, class uint32). metric:
// 0=euclidean(squared), 1=cityblock, 2=chessboard, 3=quasi-euclidean.
static Value bwdist_idx(const Value &BW, int metric, std::pmr::memory_resource *mr)
{
    const size_t H = BW.dims().rows(), W = BW.dims().cols();
    Value out = Value::matrix(H, W, ValueType::UINT32, mr);
    if (H == 0 || W == 0) return out;
    struct FG { int r, c; uint32_t lin; };
    std::vector<FG> fg;
    for (size_t c = 0; c < W; ++c)            // column-major → ascending linidx
        for (size_t r = 0; r < H; ++r)
            if (BW.elemAsDouble(c * H + r) != 0.0)
                fg.push_back({(int)r, (int)c, (uint32_t)(c * H + r + 1)});
    uint32_t *od = out.uint32DataMut();
    const double q = std::sqrt(2.0) - 1.0;
    for (size_t c = 0; c < W; ++c)
        for (size_t r = 0; r < H; ++r) {
            double best = std::numeric_limits<double>::infinity();
            uint32_t bi = 0;
            for (const auto &p : fg) {
                const double dr = std::abs((double)(long)r - p.r);
                const double dc = std::abs((double)(long)c - p.c);
                double dd;
                switch (metric) {
                    case 1:  dd = dr + dc; break;
                    case 2:  dd = std::max(dr, dc); break;
                    case 3:  dd = std::max(dr, dc) + q * std::min(dr, dc); break;
                    default: dd = dr * dr + dc * dc; break;   // euclidean (sq)
                }
                if (dd < best - 1e-12) { best = dd; bi = p.lin; }
            }
            od[c * H + r] = bi;
        }
    return out;
}

void bwdist_reg(Span<const Value> args, size_t nargout,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("bwdist: requires (BW)",
                    0, 0, "bwdist", "", "numkit:bwdist:nargin");
    auto *mr = ctx.engine->resource();

    // Optional distance metric (default 'euclidean'), case-insensitive.
    std::string method = "euclidean";
    if (args.size() >= 2 && (args[1].isChar() || args[1].isString())) {
        method = args[1].toString();
        for (char &c : method)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    int metric;
    if (method == "euclidean") {
        outs[0] = bwdist(args[0], mr);                          metric = 0;
    } else if (method == "cityblock") {
        outs[0] = bwdistChamfer(args[0], 1.0, 0.0, false, mr);  metric = 1;
    } else if (method == "chessboard") {
        outs[0] = bwdistChamfer(args[0], 1.0, 1.0, true, mr);   metric = 2;
    } else if (method == "quasi-euclidean") {
        outs[0] = bwdistChamfer(args[0], 1.0, std::sqrt(2.0), true, mr);
        metric = 3;
    } else {
        throw Error("bwdist: method must be 'euclidean', 'cityblock', "
                    "'chessboard', or 'quasi-euclidean'",
                    0, 0, "bwdist", "", "numkit:bwdist:badMethod");
    }

    // 2nd output IDX (nearest-foreground-pixel linear index) on request.
    if (nargout >= 2)
        outs[1] = bwdist_idx(args[0], metric, mr);
}

void roicolor_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("roicolor: requires (A, low, high) or (A, v)",
                    0, 0, "roicolor", "", "numkit:roicolor:nargin");
    auto *mr = ctx.engine->resource();
    if (args.size() >= 3) {
        outs[0] = roicolor(args[0], args[1], args[2].toScalar(), /*is_range=*/true, mr);
    } else {
        outs[0] = roicolor(args[0], args[1], 0.0, /*is_range=*/false, mr);
    }
}

void fchcode_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("fchcode: requires (bound)",
                    0, 0, "fchcode", "", "numkit:fchcode:nargin");
    outs[0] = fchcode(args[0], ctx.engine->resource());
}

void bwareafilt_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("bwareafilt: requires (BW, range|n [, keep] [, conn])",
                    0, 0, "bwareafilt", "", "numkit:bwareafilt:nargin");
    auto *mr = ctx.engine->resource();
    double lo = 0.0, hi = std::numeric_limits<double>::infinity();
    size_t n_keep = 0;
    bool keep_largest = true;
    int conn = 8;

    const Value &v = args[1];
    if (v.numel() >= 2) {
        // Range form.
        lo = v.elemAsDouble(0);
        hi = v.elemAsDouble(1);
    } else {
        n_keep = static_cast<size_t>(v.toScalar());
    }

    // Parse remaining args: optional keep_str + conn (any order valid?
    // Octave docs: keep before conn. Numeric → conn, string → keep.).
    for (size_t i = 2; i < args.size(); ++i) {
        const Value &a = args[i];
        if (a.isEmpty()) continue;
        if (a.isChar() || a.isString()) {
            const std::string s = a.toString();
            std::string lo_s;
            lo_s.reserve(s.size());
            for (char c : s) lo_s.push_back(static_cast<char>(std::tolower(c)));
            if      (lo_s == "largest")  keep_largest = true;
            else if (lo_s == "smallest") keep_largest = false;
            else throw Error("bwareafilt: keep must be 'largest' or 'smallest'",
                             0, 0, "bwareafilt", "", "numkit:bwareafilt:keep");
        } else if (a.numel() == 1) {
            conn = static_cast<int>(a.toScalar());
        }
    }

    outs[0] = bwareafilt(args[0], lo, hi, n_keep, keep_largest, conn, mr);
}

void bwselect_reg(Span<const Value> args, size_t nargout,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("bwselect: requires (BW, cols, rows[, conn])",
                    0, 0, "bwselect", "", "numkit:bwselect:nargin");
    int conn = 8;
    if (args.size() >= 4 && !args[3].isEmpty())
        conn = static_cast<int>(args[3].toScalar());
    auto [m, idx] = bwselect(args[0], args[1], args[2], conn, ctx.engine->resource());
    outs[0] = std::move(m);
    if (nargout > 1) outs[1] = std::move(idx);
}

void bweuler_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("bweuler: requires (BW [, n])",
                    0, 0, "bweuler", "", "numkit:bweuler:nargin");
    int conn = 8;
    if (args.size() >= 2 && !args[1].isEmpty())
        conn = static_cast<int>(args[1].toScalar());
    outs[0] = bweuler(args[0], conn, ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::image
