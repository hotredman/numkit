// toolboxes/image/src/object/object_reg.cpp
//
// Register half of the image object/region builtins: the CallContext wrappers
// delegating to the engine-free compute in object.cpp. library.cpp
// forward-declares + registers these by name. (Two detail blocks, mirroring
// the interleaved layout of the compute TU.)
//
// Phase 2b compute/register split — see project_layering_refactor memory.

#include <numkit/image/object/object.hpp>
#include <numkit/image/filter/filter.hpp>
#include <numkit/image/type_convert/type_convert.hpp>

#include <numkit/core/engine.hpp>   // CallContext, Span, ctx.engine->resource()
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include "object_detail.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <string>
#include <utility>

namespace numkit::image {

namespace detail {

namespace {
std::string parse_method(Span<const Value> args, size_t i, const std::string &def) {
    if (i < args.size() && (args[i].isChar() || args[i].isString())) {
        // MATLAB method names ('Canny', 'Sobel', 'Roberts', …) are
        // case-insensitive; downstream comparisons are all lowercase.
        std::string s = args[i].toString();
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
        return s;
    }
    return def;
}
}

void imgradientxy_reg(Span<const Value> args, size_t nargout,
                      Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imgradientxy: requires (I[, method])", 0, 0,
                    "imgradientxy", "", "numkit:imgradientxy:nargin");
    const auto m = parse_method(args, 1, "sobel");
    auto [Gx, Gy] = imgradientxy(args[0], m, ctx.engine->resource());
    outs[0] = std::move(Gx);
    if (nargout > 1) outs[1] = std::move(Gy);
}

void imgradient_reg(Span<const Value> args, size_t nargout,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imgradient: requires (I[, method])", 0, 0,
                    "imgradient", "", "numkit:imgradient:nargin");
    const auto m = parse_method(args, 1, "sobel");
    if (nargout > 1) {
        auto [Gmag, Gdir] = imgradient(args[0], m, ctx.engine->resource());
        outs[0] = std::move(Gmag);
        outs[1] = std::move(Gdir);
    } else {
        // Magnitude only — skip the per-pixel atan2 direction.
        outs[0] = imgradient_mag(args[0], m, ctx.engine->resource());
    }
}

void imgradientxyz_reg(Span<const Value> args, size_t nargout,
                       Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imgradientxyz: requires (V[, method])",
                    0, 0, "imgradientxyz", "", "numkit:imgradientxyz:nargin");
    const auto m = parse_method(args, 1, "sobel");
    auto [Gx, Gy, Gz] = imgradientxyz(args[0], m, ctx.engine->resource());
    outs[0] = std::move(Gx);
    if (nargout > 1) outs[1] = std::move(Gy);
    if (nargout > 2) outs[2] = std::move(Gz);
}

void imgradient3_reg(Span<const Value> args, size_t nargout,
                     Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("imgradient3: requires (V[, method]) or (Gx, Gy, Gz)",
                    0, 0, "imgradient3", "", "numkit:imgradient3:nargin");
    auto *mr = ctx.engine->resource();
    // Detect (Gx, Gy, Gz) form: three numeric args, NO string arg.
    if (args.size() == 3
        && !args[0].isChar() && !args[0].isString()
        && !args[1].isChar() && !args[1].isString()
        && !args[2].isChar() && !args[2].isString()
        && args[0].dims().is3D() && args[1].dims().is3D() && args[2].dims().is3D()) {
        auto [Gmag, Gaz, Gelev] =
            imgradient3_from_grads(args[0], args[1], args[2], mr);
        outs[0] = std::move(Gmag);
        if (nargout > 1) outs[1] = std::move(Gaz);
        if (nargout > 2) outs[2] = std::move(Gelev);
        return;
    }
    const auto m = parse_method(args, 1, "sobel");
    auto [Gmag, Gaz, Gelev] = imgradient3(args[0], m, mr);
    outs[0] = std::move(Gmag);
    if (nargout > 1) outs[1] = std::move(Gaz);
    if (nargout > 2) outs[2] = std::move(Gelev);
}

void edge_reg(Span<const Value> args, size_t /*nargout*/,
              Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("edge: requires (I[, method, thresh])", 0, 0, "edge", "",
                    "numkit:edge:nargin");
    const auto m = parse_method(args, 1, "sobel");
    double t_lo = std::nan(""), t_hi = std::nan("");
    if (args.size() >= 3 && !args[2].isEmpty() && !(args[2].isChar() || args[2].isString())) {
        const Value &v = args[2];
        if (v.numel() == 1) t_lo = v.toScalar();
        else if (v.numel() >= 2) {
            t_lo = v.elemAsDouble(0);
            t_hi = v.elemAsDouble(1);
        }
    }
    outs[0] = edge(args[0], m, t_lo, t_hi, ctx.engine->resource());
}

} // namespace detail

namespace detail {

void cornermetric_reg(Span<const Value> args, std::size_t /*nargout*/,
                      Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("cornermetric: requires (I [, METHOD] [, NV...])",
                    0, 0, "cornermetric", "", "numkit:cornermetric:nargin");
    auto *mr = ctx.engine->resource();
    const Value &I = args[0];
    auto is_string = [](const Value &v) { return v.isChar() || v.isString(); };

    std::string method = "Harris";
    double sensitivity = 0.04;
    Value filter_coef;
    std::size_t i = 1;
    if (i < args.size() && is_string(args[i])) {
        std::string m = args[i].toString();
        std::string mlo;
        for (char ch : m)
            mlo += static_cast<char>(std::tolower(
                static_cast<unsigned char>(ch)));
        if (m == "Harris" || m == "MinimumEigenvalue") {
            method = m;
            ++i;
        } else if (mlo == "sensitivityfactor" || mlo == "filtercoefficients") {
            // It's an NV pair name — leave i for NV parsing below.
        } else {
            throw Error("cornermetric: METHOD must be 'Harris' or "
                        "'MinimumEigenvalue' (got '" + m + "')",
                        0, 0, "cornermetric", "",
                        "numkit:cornermetric:method");
        }
    }
    while (i + 1 < args.size()) {
        if (!is_string(args[i]))
            throw Error("cornermetric: expected NV-pair name",
                        0, 0, "cornermetric", "", "numkit:cornermetric:badNv");
        std::string name = args[i].toString();
        std::string nlo;
        for (char ch : name)
            nlo += static_cast<char>(std::tolower(
                static_cast<unsigned char>(ch)));
        if (nlo == "filtercoefficients") {
            filter_coef = args[i + 1];
        } else if (nlo == "sensitivityfactor") {
            sensitivity = args[i + 1].toScalar();
        } else {
            throw Error("cornermetric: unknown option '" + name + "'",
                        0, 0, "cornermetric", "",
                        "numkit:cornermetric:unknownNv");
        }
        i += 2;
    }
    outs[0] = cornermetric(I, method, sensitivity, filter_coef, mr);
}

void hough_reg(Span<const Value> args, std::size_t nargout,
               Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("hough: requires (BW [, 'RhoResolution', val] "
                    "[, 'Theta', vec])",
                    0, 0, "hough", "", "numkit:hough:nargin");
    auto *mr = ctx.engine->resource();
    double rho_res = 1.0;
    Value theta_deg;
    auto is_string = [](const Value &v) { return v.isChar() || v.isString(); };
    std::size_t i = 1;
    while (i + 1 < args.size()) {
        if (!is_string(args[i]))
            throw Error("hough: expected NV-pair name",
                        0, 0, "hough", "", "numkit:hough:badNv");
        std::string name = args[i].toString();
        std::string nlo;
        for (char ch : name)
            nlo += static_cast<char>(std::tolower(
                static_cast<unsigned char>(ch)));
        if (nlo.compare(0, std::min<std::size_t>(nlo.size(), 4), "rhor") == 0) {
            rho_res = args[i + 1].toScalar();
        } else if (nlo == "theta") {
            theta_deg = args[i + 1];
        } else if (nlo.compare(0, std::min<std::size_t>(nlo.size(), 6), "thetar") == 0) {
            // 'ThetaResolution' (legacy): build theta = -90:tr:89.
            const double tr = args[i + 1].toScalar();
            const int n = static_cast<int>(std::ceil(90.0 / tr));
            const double step = 90.0 / n;
            std::pmr::vector<double> tv(mr);
            for (int k = -n; k < n; ++k) tv.push_back(k * step);
            theta_deg = Value::matrix(1, tv.size(), ValueType::DOUBLE, mr);
            for (std::size_t kk = 0; kk < tv.size(); ++kk)
                theta_deg.doubleDataMut()[kk] = tv[kk];
        } else {
            throw Error("hough: unknown option '" + name + "'",
                        0, 0, "hough", "", "numkit:hough:unknownNv");
        }
        i += 2;
    }
    Value H, T, R;
    hough(args[0], rho_res, theta_deg, H, T, R, mr);
    outs[0] = std::move(H);
    if (nargout >= 2) outs[1] = std::move(T);
    if (nargout >= 3) outs[2] = std::move(R);
}

void houghlines_reg(Span<const Value> args, std::size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("houghlines: requires (BW, theta, rho, peaks "
                    "[, NV...])",
                    0, 0, "houghlines", "", "numkit:houghlines:nargin");
    auto *mr = ctx.engine->resource();
    auto is_string = [](const Value &v) { return v.isChar() || v.isString(); };
    double fillgap = 20.0;
    double minlength = 40.0;
    std::size_t i = 4;
    while (i + 1 < args.size()) {
        if (!is_string(args[i]))
            throw Error("houghlines: expected NV-pair name",
                        0, 0, "houghlines", "", "numkit:houghlines:badNv");
        std::string name = args[i].toString();
        std::string nlo;
        for (char ch : name)
            nlo += static_cast<char>(std::tolower(
                static_cast<unsigned char>(ch)));
        if (nlo == "fillgap") {
            fillgap = args[i + 1].toScalar();
        } else if (nlo == "minlength") {
            minlength = args[i + 1].toScalar();
        } else {
            throw Error("houghlines: unknown option '" + name + "'",
                        0, 0, "houghlines", "",
                        "numkit:houghlines:unknownNv");
        }
        i += 2;
    }
    outs[0] = houghlines(args[0], args[1], args[2], args[3],
                         fillgap, minlength, mr);
}

void houghpeaks_reg(Span<const Value> args, std::size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("houghpeaks: requires (H [, numpeaks] [, NV...])",
                    0, 0, "houghpeaks", "", "numkit:houghpeaks:nargin");
    auto *mr = ctx.engine->resource();
    auto is_string = [](const Value &v) { return v.isChar() || v.isString(); };

    std::size_t numpeaks = 1;
    double threshold = -1.0;  // sentinel: use default
    std::size_t nhoodRho = 0, nhoodTheta = 0;
    Value theta_deg;

    std::size_t i = 1;
    if (i < args.size() && !is_string(args[i])) {
        const double npd = args[i].toScalar();
        if (!(npd > 0) || npd != std::floor(npd))
            throw Error("houghpeaks: NUMPEAKS must be a positive integer",
                        0, 0, "houghpeaks", "", "numkit:houghpeaks:numpeaks");
        numpeaks = static_cast<std::size_t>(npd);
        ++i;
    }
    while (i + 1 < args.size()) {
        if (!is_string(args[i]))
            throw Error("houghpeaks: expected NV-pair name",
                        0, 0, "houghpeaks", "", "numkit:houghpeaks:badNv");
        std::string name = args[i].toString();
        std::string nlo;
        for (char ch : name)
            nlo += static_cast<char>(std::tolower(
                static_cast<unsigned char>(ch)));
        if (nlo == "threshold") {
            threshold = args[i + 1].toScalar();
            if (!(threshold >= 0.0))
                throw Error("houghpeaks: Threshold must be non-negative",
                            0, 0, "houghpeaks", "",
                            "numkit:houghpeaks:threshold");
        } else if (nlo == "nhoodsize") {
            const Value &v = args[i + 1];
            if (v.numel() != 2)
                throw Error("houghpeaks: NHoodSize must be a 2-elem vector",
                            0, 0, "houghpeaks", "",
                            "numkit:houghpeaks:nhoodSize");
            const double a = v.elemAsDouble(0);
            const double b = v.elemAsDouble(1);
            if (!(a > 0) || !(b > 0)
             || a != std::floor(a) || b != std::floor(b)
             || static_cast<int>(a) % 2 == 0
             || static_cast<int>(b) % 2 == 0)
                throw Error("houghpeaks: NHoodSize elements must be "
                            "positive odd integers",
                            0, 0, "houghpeaks", "",
                            "numkit:houghpeaks:nhoodOdd");
            nhoodRho   = static_cast<std::size_t>(a);
            nhoodTheta = static_cast<std::size_t>(b);
        } else if (nlo == "theta") {
            theta_deg = args[i + 1];
        } else {
            throw Error("houghpeaks: unknown option '" + name + "'",
                        0, 0, "houghpeaks", "",
                        "numkit:houghpeaks:unknownNv");
        }
        i += 2;
    }
    outs[0] = houghpeaks(args[0], numpeaks, threshold,
                         nhoodRho, nhoodTheta, theta_deg, mr);
}

} // namespace detail

} // namespace numkit::image
