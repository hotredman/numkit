// toolboxes/image/src/segment/segment_reg.cpp
//
// Register half of the image segment builtins: the CallContext wrappers
// delegating to the engine-free compute in segment.cpp. library.cpp
// forward-declares + registers these by name.
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/image/segment/segment.hpp>

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

void reducepoly_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.empty())
        throw Error("reducepoly: requires (P[, tolerance])",
                    0, 0, "reducepoly", "", "numkit:reducepoly:nargin");
    double tol = 0.001;
    if (a.size() >= 2 && !a[1].isEmpty()) tol = a[1].toScalar();
    o[0] = reducepoly(a[0], tol, c.engine->resource());
}

void imoverlay_reg(Span<const Value> a, size_t, Span<Value> o,
                   CallContext &c)
{
    if (a.size() < 3)
        throw Error("imoverlay: requires (I, BW, color)",
                    0, 0, "imoverlay", "", "numkit:imoverlay:nargin");
    o[0] = imoverlay(a[0], a[1], a[2], c.engine->resource());
}

void grayconnected_reg(Span<const Value> a, size_t, Span<Value> o,
                       CallContext &c)
{
    if (a.size() < 3)
        throw Error("grayconnected: requires (I, row, col [, tol])",
                    0, 0, "grayconnected", "", "numkit:grayconnected:nargin");
    const int row = static_cast<int>(a[1].toScalar());
    const int col = static_cast<int>(a[2].toScalar());
    double tol = -1.0;
    if (a.size() >= 4 && !a[3].isEmpty()) tol = a[3].toScalar();
    o[0] = grayconnected(a[0], row, col, tol, c.engine->resource());
}

void dice_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.size() < 2)
        throw Error("dice: requires (BW1, BW2)",
                    0, 0, "dice", "", "numkit:dice:nargin");
    o[0] = dice(a[0], a[1], c.engine->resource());
}

void jaccard_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.size() < 2)
        throw Error("jaccard: requires (BW1, BW2)",
                    0, 0, "jaccard", "", "numkit:jaccard:nargin");
    o[0] = jaccard(a[0], a[1], c.engine->resource());
}

void boundarymask_reg(Span<const Value> a, size_t, Span<Value> o,
                      CallContext &c)
{
    if (a.empty())
        throw Error("boundarymask: requires (L_or_BW [, conn])",
                    0, 0, "boundarymask", "", "numkit:boundarymask:nargin");
    const int conn = (a.size() >= 2 && !a[1].isEmpty())
                     ? static_cast<int>(a[1].toScalar()) : 8;
    o[0] = boundarymask(a[0], conn, c.engine->resource());
}

// graydiffweight adapter — handles all 4 input signatures plus the
// 'RolloffFactor' / 'GrayDifferenceCutoff' name-value pairs by
// computing the scalar reference value upfront and dispatching to
// the typed entry-point.
void graydiffweight_reg(Span<const Value> a, size_t, Span<Value> o,
                        CallContext &c)
{
    if (a.size() < 2)
        throw Error("graydiffweight: requires (I, refGrayVal | MASK | "
                    "C, R [, P]) [, NV...]",
                    0, 0, "graydiffweight", "", "numkit:graydiffweight:nargin");
    auto *mr = c.engine->resource();
    const Value &I = a[0];
    const auto &d = I.dims();
    const std::size_t N = I.numel();
    const std::size_t H = d.rows();
    const std::size_t W_ = d.cols();
    const std::size_t P = d.is3D() ? d.pages() : 1;

    // Identify which signature is in play, then determine where the
    // name-value pairs start.
    double ref_gray_val = 0.0;
    std::size_t nv_start = 2;

    auto is_string = [](const Value &v) { return v.isChar() || v.isString(); };
    const bool a1_str = is_string(a[1]);

    if (!a1_str && a[1].isLogical()) {
        // (I, MASK) — mean of I over MASK = true.
        if (a[1].numel() != N)
            throw Error("graydiffweight: MASK must match I in shape",
                        0, 0, "graydiffweight", "", "numkit:graydiffweight:mask");
        long double sum = 0.0L; std::size_t cnt = 0;
        const std::uint8_t *m = a[1].logicalData();
        for (std::size_t i = 0; i < N; ++i)
            if (m[i]) { sum += I.elemAsDouble(i); ++cnt; }
        if (cnt == 0)
            throw Error("graydiffweight: MASK must contain at least one true",
                        0, 0, "graydiffweight", "", "numkit:graydiffweight:emptyMask");
        ref_gray_val = static_cast<double>(sum / static_cast<long double>(cnt));
        nv_start = 2;
    }
    else if (!a1_str && a[1].numel() == 1 &&
             (a.size() < 3 || a[2].isChar() || a[2].isString())) {
        // (I, refGrayVal) — scalar reference.
        ref_gray_val = a[1].toScalar();
        nv_start = 2;
    }
    else if (!a1_str && a.size() >= 3 && !is_string(a[2])) {
        // (I, C, R [, P]) — mean over indexed pixels.
        const Value &C = a[1], &R = a[2];
        const bool has_P = (a.size() >= 4 && !is_string(a[3]));
        const Value *Pi = has_P ? &a[3] : nullptr;
        if (C.numel() != R.numel() || (Pi && Pi->numel() != C.numel()))
            throw Error("graydiffweight: C, R [, P] must have equal length",
                        0, 0, "graydiffweight", "", "numkit:graydiffweight:crp");
        long double sum = 0.0L; std::size_t cnt = 0;
        for (std::size_t i = 0; i < C.numel(); ++i) {
            const std::size_t c1 = static_cast<std::size_t>(C.elemAsDouble(i));
            const std::size_t r1 = static_cast<std::size_t>(R.elemAsDouble(i));
            const std::size_t p1 = Pi ? static_cast<std::size_t>(Pi->elemAsDouble(i))
                                       : 1;
            if (c1 < 1 || c1 > W_ || r1 < 1 || r1 > H || p1 < 1 || p1 > P)
                throw Error("graydiffweight: (C, R [, P]) index out of range",
                            0, 0, "graydiffweight", "",
                            "numkit:graydiffweight:idx");
            // Column-major linear: r-1 + H*(c-1) + H*W*(p-1).
            const std::size_t lin = (r1 - 1) + H * (c1 - 1)
                                  + H * W_ * (p1 - 1);
            sum += I.elemAsDouble(lin);
            ++cnt;
        }
        ref_gray_val = static_cast<double>(sum / static_cast<long double>(cnt));
        nv_start = has_P ? 4 : 3;
    }
    else {
        throw Error("graydiffweight: 2nd argument must be a numeric "
                    "scalar, a logical MASK, or numeric C [, R [, P]]",
                    0, 0, "graydiffweight", "", "numkit:graydiffweight:arg2");
    }

    // Name-value pairs.
    double rolloff = 0.5;
    double cutoff  = std::numeric_limits<double>::infinity();
    std::size_t i = nv_start;
    while (i + 1 < a.size()) {
        if (!is_string(a[i]))
            throw Error("graydiffweight: expected NV-pair name string",
                        0, 0, "graydiffweight", "",
                        "numkit:graydiffweight:badNvArg");
        std::string name = a[i].toString();
        std::string nlo = name;
        for (auto &ch : nlo)
            ch = static_cast<char>(std::tolower(
                static_cast<unsigned char>(ch)));
        // Allow MATLAB-style abbreviation.
        if (nlo.compare(0, std::min<std::size_t>(nlo.size(), 4), "roll") == 0)
            rolloff = a[i + 1].toScalar();
        else if (nlo.compare(0, std::min<std::size_t>(nlo.size(), 4), "gray") == 0)
            cutoff = a[i + 1].toScalar();
        else
            throw Error("graydiffweight: unknown option '" + name + "'",
                        0, 0, "graydiffweight", "",
                        "numkit:graydiffweight:unknownNv");
        i += 2;
    }
    if (i < a.size())
        throw Error("graydiffweight: trailing unpaired NV argument",
                    0, 0, "graydiffweight", "",
                    "numkit:graydiffweight:unpaired");

    o[0] = graydiffweight(I, ref_gray_val, rolloff, cutoff, mr);
}

// gradientweight adapter — parses (I [, sigma] [, NV...]).
//   sigma: scalar (replicated) or 2-element [sigma_x sigma_y]; default 1.5.
//   'RolloffFactor': positive scalar, default 3.
//   'WeightCutoff':  scalar in [1e-3, 1], default 0.25.
void gradientweight_reg(Span<const Value> a, size_t, Span<Value> o,
                        CallContext &c)
{
    if (a.empty())
        throw Error("gradientweight: requires (I [, sigma] [, NV...])",
                    0, 0, "gradientweight", "", "numkit:gradientweight:nargin");
    auto *mr = c.engine->resource();
    const Value &I = a[0];

    double sigma_x = 1.5, sigma_y = 1.5;
    double rolloff = 3.0;
    double cutoff  = 0.25;

    std::size_t nv_start = 1;
    auto is_string = [](const Value &v) { return v.isChar() || v.isString(); };

    // Optional sigma argument.
    if (a.size() >= 2 && !is_string(a[1])) {
        const Value &s = a[1];
        const std::size_t ns = s.numel();
        if (ns == 1) {
            sigma_x = sigma_y = s.toScalar();
        } else if (ns == 2) {
            sigma_x = s.elemAsDouble(0);
            sigma_y = s.elemAsDouble(1);
        } else {
            throw Error("gradientweight: sigma must be a scalar or "
                        "2-element vector",
                        0, 0, "gradientweight", "",
                        "numkit:gradientweight:sigmaSize");
        }
        nv_start = 2;
    }

    // Name-value pairs.
    std::size_t i = nv_start;
    while (i + 1 < a.size()) {
        if (!is_string(a[i]))
            throw Error("gradientweight: expected NV-pair name string",
                        0, 0, "gradientweight", "",
                        "numkit:gradientweight:badNvArg");
        std::string name = a[i].toString();
        std::string nlo = name;
        for (auto &ch : nlo)
            ch = static_cast<char>(std::tolower(
                static_cast<unsigned char>(ch)));
        // MATLAB-style abbreviation: "RolloffFactor" / "WeightCutoff".
        if (nlo.compare(0, std::min<std::size_t>(nlo.size(), 4), "roll") == 0)
            rolloff = a[i + 1].toScalar();
        else if (nlo.compare(0, std::min<std::size_t>(nlo.size(), 6), "weight") == 0
              || nlo.compare(0, std::min<std::size_t>(nlo.size(), 3), "cut") == 0)
            cutoff = a[i + 1].toScalar();
        else
            throw Error("gradientweight: unknown option '" + name + "'",
                        0, 0, "gradientweight", "",
                        "numkit:gradientweight:unknownNv");
        i += 2;
    }
    if (i < a.size())
        throw Error("gradientweight: trailing unpaired NV argument",
                    0, 0, "gradientweight", "",
                    "numkit:gradientweight:unpaired");

    o[0] = gradientweight(I, sigma_x, sigma_y, rolloff, cutoff, mr);
}

// regionfill adapter — both (I, MASK) and (I, X, Y) polygon forms.
void regionfill_reg(Span<const Value> a, size_t, Span<Value> o,
                    CallContext &c)
{
    if (a.size() < 2)
        throw Error("regionfill: requires (I, MASK) or (I, X, Y)",
                    0, 0, "regionfill", "", "numkit:regionfill:nargin");
    auto *mr = c.engine->resource();
    if (a.size() == 2) {
        o[0] = regionfill(a[0], a[1], mr);
        return;
    }
    // (I, X, Y) form — build mask via poly2mask using I's H/W.
    const Value &I = a[0];
    const std::size_t H = I.dims().rows();
    const std::size_t W = I.dims().cols();
    Value mask = poly2mask(a[1], a[2], H, W, mr);
    o[0] = regionfill(I, mask, mr);
}

// roipoly adapter — handles the 4 programmatic signatures and the
// 1/2/3/4/5-output forms. Interactive (1 / 2 / 0-arg) variants throw.
void roipoly_reg(Span<const Value> a, size_t nargout, Span<Value> o,
                 CallContext &c)
{
    if (a.size() < 3 || a.size() > 6)
        throw Error("roipoly: interactive forms not supported; use "
                    "(A, xi, yi) | (M, N, xi, yi) | (x, y, A, xi, yi) "
                    "| (x, y, M, N, xi, yi)",
                    0, 0, "roipoly", "", "numkit:roipoly:nargin");
    auto *mr = c.engine->resource();

    // Helper: pull [lo, hi] from a Value that's either a 2-elem extent
    // vector or a scalar (treated as a degenerate extent).
    auto extent = [&](const Value &v, double dflt_lo, double dflt_hi,
                      double &lo, double &hi) {
        if (v.numel() == 0) { lo = dflt_lo; hi = dflt_hi; }
        else if (v.numel() == 1) {
            lo = hi = v.toScalar();
        } else {
            lo = v.elemAsDouble(0);
            hi = v.elemAsDouble(v.numel() - 1);
        }
    };

    double xlo = 0, xhi = 0, ylo = 0, yhi = 0;
    std::size_t M = 0, N = 0;
    Value xi, yi;

    switch (a.size()) {
        case 3: {
            // (A, xi, yi)
            const Value &A = a[0];
            M = A.dims().rows();
            N = A.dims().cols();
            xlo = 1.0;  xhi = static_cast<double>(N);
            ylo = 1.0;  yhi = static_cast<double>(M);
            xi = a[1]; yi = a[2];
            break;
        }
        case 4: {
            // (M, N, xi, yi)
            const double Md = a[0].toScalar();
            const double Nd = a[1].toScalar();
            if (Md < 0 || Nd < 0 || Md != std::floor(Md) || Nd != std::floor(Nd))
                throw Error("roipoly: M and N must be non-negative integers",
                            0, 0, "roipoly", "", "numkit:roipoly:mn");
            M = static_cast<std::size_t>(Md);
            N = static_cast<std::size_t>(Nd);
            xlo = 1.0;  xhi = static_cast<double>(N);
            ylo = 1.0;  yhi = static_cast<double>(M);
            xi = a[2]; yi = a[3];
            break;
        }
        case 5: {
            // (x, y, A, xi, yi)
            const Value &A = a[2];
            M = A.dims().rows();
            N = A.dims().cols();
            extent(a[0], 1.0, static_cast<double>(N), xlo, xhi);
            extent(a[1], 1.0, static_cast<double>(M), ylo, yhi);
            xi = a[3]; yi = a[4];
            break;
        }
        case 6: {
            // (x, y, M, N, xi, yi)
            const double Md = a[2].toScalar();
            const double Nd = a[3].toScalar();
            if (Md < 0 || Nd < 0 || Md != std::floor(Md) || Nd != std::floor(Nd))
                throw Error("roipoly: M and N must be non-negative integers",
                            0, 0, "roipoly", "", "numkit:roipoly:mn");
            M = static_cast<std::size_t>(Md);
            N = static_cast<std::size_t>(Nd);
            extent(a[0], 1.0, static_cast<double>(N), xlo, xhi);
            extent(a[1], 1.0, static_cast<double>(M), ylo, yhi);
            xi = a[4]; yi = a[5];
            break;
        }
    }

    // Build the auto-closed xi/yi for the multi-output forms (matches
    // MATLAB's behaviour: outputs the closed polygon as a column vector).
    const std::size_t n0 = xi.numel();
    std::pmr::vector<double> xc(mr), yc(mr);
    xc.reserve(n0 + 1);
    yc.reserve(n0 + 1);
    for (std::size_t i = 0; i < n0; ++i) {
        xc.push_back(xi.elemAsDouble(i));
        yc.push_back(yi.elemAsDouble(i));
    }
    if (n0 > 0 && (xc.front() != xc.back() || yc.front() != yc.back())) {
        xc.push_back(xc.front());
        yc.push_back(yc.front());
    }
    const std::size_t nc = xc.size();
    Value xiOut = Value::matrix(nc, 1, ValueType::DOUBLE, mr);
    Value yiOut = Value::matrix(nc, 1, ValueType::DOUBLE, mr);
    for (std::size_t i = 0; i < nc; ++i) {
        xiOut.doubleDataMut()[i] = xc[i];
        yiOut.doubleDataMut()[i] = yc[i];
    }

    Value BW = roipoly(xlo, xhi, ylo, yhi, M, N, xi, yi, mr);

    // Output dispatch.
    Value xDataOut = Value::matrix(1, 2, ValueType::DOUBLE, mr);
    xDataOut.doubleDataMut()[0] = xlo; xDataOut.doubleDataMut()[1] = xhi;
    Value yDataOut = Value::matrix(1, 2, ValueType::DOUBLE, mr);
    yDataOut.doubleDataMut()[0] = ylo; yDataOut.doubleDataMut()[1] = yhi;

    switch (nargout) {
        case 0: case 1: o[0] = std::move(BW); break;
        case 2: o[0] = std::move(BW); o[1] = std::move(xiOut); break;
        case 3: o[0] = std::move(BW); o[1] = std::move(xiOut);
                o[2] = std::move(yiOut); break;
        case 4: o[0] = std::move(xDataOut); o[1] = std::move(yDataOut);
                o[2] = std::move(BW); o[3] = std::move(xiOut); break;
        case 5: o[0] = std::move(xDataOut); o[1] = std::move(yDataOut);
                o[2] = std::move(BW); o[3] = std::move(xiOut);
                o[4] = std::move(yiOut); break;
        default:
            throw Error("roipoly: too many output arguments (max 5)",
                        0, 0, "roipoly", "", "numkit:roipoly:tooManyOutputs");
    }
}

// graydist adapter — handles the 4 input forms + optional METHOD.
void graydist_reg(Span<const Value> a, size_t, Span<Value> o,
                  CallContext &c)
{
    if (a.size() < 2)
        throw Error("graydist: requires (A, mask | ind | C, R "
                    "[, method])",
                    0, 0, "graydist", "", "numkit:graydist:nargin");
    auto *mr = c.engine->resource();
    auto is_string = [](const Value &v) { return v.isChar() || v.isString(); };

    // Strip trailing method string if present.
    std::string method = "chessboard";
    std::size_t nargs = a.size();
    if (is_string(a[nargs - 1])) {
        method = a[nargs - 1].toString();
        std::string lo;
        for (char ch : method)
            lo += static_cast<char>(std::tolower(
                static_cast<unsigned char>(ch)));
        method = lo;
        --nargs;
    }

    const Value &A = a[0];
    const std::size_t H = A.dims().rows();
    const std::size_t W = A.dims().cols();

    // Build 1-based linear seed indices.
    std::pmr::vector<double> indices(mr);
    if (nargs == 2) {
        const Value &arg2 = a[1];
        if (arg2.isLogical()) {
            // Mask: same size as A.
            if (arg2.numel() != H * W)
                throw Error("graydist: MASK must be the same size as A",
                            0, 0, "graydist", "", "numkit:graydist:maskSize");
            const std::uint8_t *m = arg2.logicalData();
            for (std::size_t i = 0; i < arg2.numel(); ++i)
                if (m[i]) indices.push_back(static_cast<double>(i + 1));
        } else {
            // Linear indices.
            indices.reserve(arg2.numel());
            for (std::size_t i = 0; i < arg2.numel(); ++i)
                indices.push_back(arg2.elemAsDouble(i));
        }
    } else if (nargs == 3) {
        // (A, C, R) — column-then-row coordinates.
        const Value &C = a[1];
        const Value &R = a[2];
        if (C.numel() != R.numel())
            throw Error("graydist: C and R must have equal length",
                        0, 0, "graydist", "", "numkit:graydist:cr");
        indices.reserve(C.numel());
        for (std::size_t i = 0; i < C.numel(); ++i) {
            const std::size_t cc = static_cast<std::size_t>(C.elemAsDouble(i));
            const std::size_t rr = static_cast<std::size_t>(R.elemAsDouble(i));
            if (cc < 1 || cc > W || rr < 1 || rr > H)
                throw Error("graydist: (C, R) out of bounds",
                            0, 0, "graydist", "", "numkit:graydist:crBounds");
            // Column-major 1-based linear index.
            const std::size_t lin = (cc - 1) * H + rr;  // 1-based
            indices.push_back(static_cast<double>(lin));
        }
    } else {
        throw Error("graydist: too many positional arguments",
                    0, 0, "graydist", "", "numkit:graydist:nargin");
    }

    Value seedVec = Value::matrix(indices.size(), 1, ValueType::DOUBLE, mr);
    for (std::size_t i = 0; i < indices.size(); ++i)
        seedVec.doubleDataMut()[i] = indices[i];
    o[0] = graydist(A, seedVec, method, mr);
}

// bwdistgeodesic adapter — same input parsing as graydist.
void bwdistgeodesic_reg(Span<const Value> a, size_t, Span<Value> o,
                        CallContext &c)
{
    if (a.size() < 2)
        throw Error("bwdistgeodesic: requires (BW, mask | ind | C, R "
                    "[, method])",
                    0, 0, "bwdistgeodesic", "",
                    "numkit:bwdistgeodesic:nargin");
    auto *mr = c.engine->resource();
    auto is_string = [](const Value &v) { return v.isChar() || v.isString(); };

    std::string method = "chessboard";
    std::size_t nargs = a.size();
    if (is_string(a[nargs - 1])) {
        method = a[nargs - 1].toString();
        std::string lo;
        for (char ch : method)
            lo += static_cast<char>(std::tolower(
                static_cast<unsigned char>(ch)));
        method = lo;
        --nargs;
    }

    const Value &BW = a[0];
    const std::size_t H = BW.dims().rows();
    const std::size_t W = BW.dims().cols();

    std::pmr::vector<double> indices(mr);
    if (nargs == 2) {
        const Value &arg2 = a[1];
        if (arg2.isLogical()) {
            if (arg2.numel() != H * W)
                throw Error("bwdistgeodesic: MASK must be the same size as BW",
                            0, 0, "bwdistgeodesic", "",
                            "numkit:bwdistgeodesic:maskSize");
            const std::uint8_t *m = arg2.logicalData();
            for (std::size_t i = 0; i < arg2.numel(); ++i)
                if (m[i]) indices.push_back(static_cast<double>(i + 1));
        } else {
            indices.reserve(arg2.numel());
            for (std::size_t i = 0; i < arg2.numel(); ++i)
                indices.push_back(arg2.elemAsDouble(i));
        }
    } else if (nargs == 3) {
        const Value &C = a[1];
        const Value &R = a[2];
        if (C.numel() != R.numel())
            throw Error("bwdistgeodesic: C and R must have equal length",
                        0, 0, "bwdistgeodesic", "",
                        "numkit:bwdistgeodesic:cr");
        indices.reserve(C.numel());
        for (std::size_t i = 0; i < C.numel(); ++i) {
            const std::size_t cc = static_cast<std::size_t>(C.elemAsDouble(i));
            const std::size_t rr = static_cast<std::size_t>(R.elemAsDouble(i));
            if (cc < 1 || cc > W || rr < 1 || rr > H)
                throw Error("bwdistgeodesic: (C, R) out of bounds",
                            0, 0, "bwdistgeodesic", "",
                            "numkit:bwdistgeodesic:crBounds");
            const std::size_t lin = (cc - 1) * H + rr;
            indices.push_back(static_cast<double>(lin));
        }
    } else {
        throw Error("bwdistgeodesic: too many positional arguments",
                    0, 0, "bwdistgeodesic", "",
                    "numkit:bwdistgeodesic:nargin");
    }

    Value seedVec = Value::matrix(indices.size(), 1, ValueType::DOUBLE, mr);
    for (std::size_t i = 0; i < indices.size(); ++i)
        seedVec.doubleDataMut()[i] = indices[i];
    o[0] = bwdistgeodesic(BW, seedVec, method, mr);
}

void poly2mask_reg(Span<const Value> a, size_t, Span<Value> o,
                   CallContext &c)
{
    if (a.size() < 4)
        throw Error("poly2mask: requires (X, Y, M, N)",
                    0, 0, "poly2mask", "", "numkit:poly2mask:nargin");
    auto *mr = c.engine->resource();
    const double Md = a[2].toScalar();
    const double Nd = a[3].toScalar();
    if (!std::isfinite(Md) || !std::isfinite(Nd)
     || Md < 0.0 || Nd < 0.0
     || Md != std::floor(Md) || Nd != std::floor(Nd))
        throw Error("poly2mask: M and N must be non-negative integers",
                    0, 0, "poly2mask", "", "numkit:poly2mask:mn");
    o[0] = poly2mask(a[0], a[1],
                     static_cast<std::size_t>(Md),
                     static_cast<std::size_t>(Nd), mr);
}

void label2idx_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.empty())
        throw Error("label2idx: requires (L)",
                    0, 0, "label2idx", "", "numkit:label2idx:nargin");
    o[0] = label2idx(a[0], c.engine->resource());
}

} // namespace detail

} // namespace numkit::image
