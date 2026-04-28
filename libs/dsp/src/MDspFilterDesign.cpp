// libs/dsp/src/MDspFilterDesign.cpp

#include <numkit/m/dsp/MDspFilterDesign.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MTypes.hpp>

#include "MDspHelpers.hpp"   // Complex typedef

#define _USE_MATH_DEFINES
#include <cmath>
#include <complex>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::m::dsp {

// ── Internal algorithm helpers ────────────────────────────────────────

namespace {

std::vector<Complex> butterworthPoles(int N)
{
    std::vector<Complex> poles;
    poles.reserve(N);
    for (int k = 0; k < N; ++k) {
        const double theta = M_PI * (2.0 * k + N + 1) / (2.0 * N);
        poles.emplace_back(std::cos(theta), std::sin(theta));
    }
    return poles;
}

std::vector<double> expandPoly(const std::vector<Complex> &roots)
{
    const int n = static_cast<int>(roots.size());
    std::vector<Complex> poly(n + 1, Complex(0.0, 0.0));
    poly[0] = Complex(1.0, 0.0);
    for (int i = 0; i < n; ++i)
        for (int j = i + 1; j >= 1; --j)
            poly[j] = poly[j] - roots[i] * poly[j - 1];
    std::vector<double> result(n + 1);
    for (int i = 0; i <= n; ++i)
        result[i] = poly[i].real();
    return result;
}

void bilinearTransform(const std::vector<Complex> &sPoles,
                       double Wn,
                       std::vector<double> &bOut,
                       std::vector<double> &aOut)
{
    const int N = static_cast<int>(sPoles.size());

    std::vector<Complex> zPoles(N);
    for (int i = 0; i < N; ++i) {
        const Complex sp = sPoles[i] * Wn;
        zPoles[i] = (1.0 + sp / 2.0) / (1.0 - sp / 2.0);
    }

    std::vector<Complex> zZeros(N, Complex(-1.0, 0.0));

    aOut = expandPoly(zPoles);
    bOut = expandPoly(zZeros);

    Complex numDC(0, 0), denDC(0, 0);
    for (double v : bOut)
        numDC += v;
    for (double v : aOut)
        denDC += v;
    const double dcGain = std::abs(numDC / denDC);
    if (dcGain > 0.0)
        for (double &v : bOut)
            v /= dcGain;
}

void lpToHp(std::vector<double> &b, std::vector<double> &a)
{
    for (size_t i = 0; i < b.size(); ++i)
        if (i % 2 == 1)
            b[i] = -b[i];
    for (size_t i = 0; i < a.size(); ++i)
        if (i % 2 == 1)
            a[i] = -a[i];

    Complex numNyq(0, 0), denNyq(0, 0);
    for (size_t i = 0; i < b.size(); ++i)
        numNyq += b[i] * ((i % 2 == 0) ? 1.0 : -1.0);
    for (size_t i = 0; i < a.size(); ++i)
        denNyq += a[i] * ((i % 2 == 0) ? 1.0 : -1.0);
    const double nyqGain = std::abs(numNyq / denNyq);
    if (nyqGain > 0.0)
        for (double &v : b)
            v /= nyqGain;
}

} // anonymous namespace

// ── butter ────────────────────────────────────────────────────────────
std::tuple<MValue, MValue>
butter(Allocator &alloc, int N, double Wn, const std::string &type)
{
    if (Wn <= 0.0 || Wn >= 1.0)
        throw MError("butter: Wn must be between 0 and 1",
                     0, 0, "butter", "", "m:butter:badWn");
    if (type != "low" && type != "high")
        throw MError("butter: type must be 'low' or 'high'",
                     0, 0, "butter", "", "m:butter:badType");

    const double Wa = 2.0 * std::tan(M_PI * Wn / 2.0);
    auto sPoles = butterworthPoles(N);

    std::vector<double> b, a;
    bilinearTransform(sPoles, Wa, b, a);

    if (type == "high")
        lpToHp(b, a);

    auto bv = MValue::matrix(1, b.size(), MType::DOUBLE, &alloc);
    auto av = MValue::matrix(1, a.size(), MType::DOUBLE, &alloc);
    for (size_t i = 0; i < b.size(); ++i)
        bv.doubleDataMut()[i] = b[i];
    for (size_t i = 0; i < a.size(); ++i)
        av.doubleDataMut()[i] = a[i];

    return std::make_tuple(std::move(bv), std::move(av));
}

// ── fir1 ──────────────────────────────────────────────────────────────
MValue fir1(Allocator &alloc, int N, double Wn, const std::string &type)
{
    if (Wn <= 0.0 || Wn >= 1.0)
        throw MError("fir1: Wn must be between 0 and 1",
                     0, 0, "fir1", "", "m:fir1:badWn");
    if (type != "low" && type != "high")
        throw MError("fir1: type must be 'low' or 'high'",
                     0, 0, "fir1", "", "m:fir1:badType");

    const size_t filtLen = N + 1;
    const double wc = M_PI * Wn;
    const double half = N / 2.0;

    std::vector<double> h(filtLen);
    double hSum = 0.0;

    for (size_t i = 0; i < filtLen; ++i) {
        const double n = i - half;
        const double sinc = (std::abs(n) < 1e-12) ? wc / M_PI
                                                  : std::sin(wc * n) / (M_PI * n);
        const double win = 0.54 - 0.46 * std::cos(2.0 * M_PI * i / N);
        h[i] = sinc * win;
        hSum += h[i];
    }

    if (type == "low") {
        for (size_t i = 0; i < filtLen; ++i)
            h[i] /= hSum;
    } else { // "high"
        for (size_t i = 0; i < filtLen; ++i)
            h[i] /= hSum;
        for (size_t i = 0; i < filtLen; ++i)
            h[i] = -h[i];
        h[static_cast<size_t>(half)] += 1.0;
    }

    auto bv = MValue::matrix(1, filtLen, MType::DOUBLE, &alloc);
    for (size_t i = 0; i < filtLen; ++i)
        bv.doubleDataMut()[i] = h[i];
    return bv;
}

// ── freqz ─────────────────────────────────────────────────────────────
std::tuple<MValue, MValue>
freqz(Allocator &alloc, const MValue &b, const MValue &a, size_t npts)
{
    const double *bd = b.doubleData();
    const double *ad = a.doubleData();
    const size_t nb = b.numel(), na = a.numel();

    auto W = MValue::matrix(npts, 1, MType::DOUBLE, &alloc);
    auto H = MValue::complexMatrix(npts, 1, &alloc);

    for (size_t k = 0; k < npts; ++k) {
        const double w = M_PI * k / (npts - 1);
        W.doubleDataMut()[k] = w;

        const Complex ejw(std::cos(w), -std::sin(w));
        Complex num(0, 0), den(0, 0);
        Complex ejwk(1, 0);
        for (size_t i = 0; i < nb; ++i) {
            num += bd[i] * ejwk;
            ejwk *= ejw;
        }
        ejwk = Complex(1, 0);
        for (size_t i = 0; i < na; ++i) {
            den += ad[i] * ejwk;
            ejwk *= ejw;
        }
        H.complexDataMut()[k] = num / den;
    }

    return std::make_tuple(std::move(H), std::move(W));
}

// ── phasez ────────────────────────────────────────────────────────────
namespace {

// Local unwrap (default tolerance π) — keeps phasez free of inter-file
// dependencies on MDspTransform.
void unwrapInPlace(double *p, size_t n)
{
    if (n < 2) return;
    constexpr double kPi  = M_PI;
    constexpr double k2Pi = 2.0 * M_PI;
    double offset = 0.0;
    for (size_t i = 1; i < n; ++i) {
        const double diff = p[i] + offset - p[i - 1];
        if (diff >  kPi) offset -= k2Pi;
        else if (diff < -kPi) offset += k2Pi;
        p[i] += offset;
    }
}

} // namespace

std::tuple<MValue, MValue>
phasez(Allocator &alloc, const MValue &b, const MValue &a, size_t npts)
{
    auto [H, W] = freqz(alloc, b, a, npts);
    auto phi = MValue::matrix(npts, 1, MType::DOUBLE, &alloc);
    const Complex *hd = H.complexData();
    double *pd = phi.doubleDataMut();
    for (size_t k = 0; k < npts; ++k)
        pd[k] = std::atan2(hd[k].imag(), hd[k].real());
    unwrapInPlace(pd, npts);
    return std::make_tuple(std::move(phi), std::move(W));
}

// ── grpdelay ──────────────────────────────────────────────────────────
std::tuple<MValue, MValue>
grpdelay(Allocator &alloc, const MValue &b, const MValue &a, size_t npts)
{
    auto [phi, W] = phasez(alloc, b, a, npts);
    auto gd = MValue::matrix(npts, 1, MType::DOUBLE, &alloc);
    const double *p = phi.doubleData();
    const double *w = W.doubleData();
    double *g = gd.doubleDataMut();
    if (npts == 0) return std::make_tuple(std::move(gd), std::move(W));
    if (npts == 1) {
        g[0] = 0.0;
        return std::make_tuple(std::move(gd), std::move(W));
    }
    // Central differences in interior; one-sided at endpoints. Group
    // delay = -d(phase)/d(omega) (negative slope of phase vs omega).
    g[0]        = -(p[1] - p[0])               / (w[1] - w[0]);
    g[npts - 1] = -(p[npts - 1] - p[npts - 2]) / (w[npts - 1] - w[npts - 2]);
    for (size_t k = 1; k + 1 < npts; ++k)
        g[k] = -(p[k + 1] - p[k - 1]) / (w[k + 1] - w[k - 1]);
    return std::make_tuple(std::move(gd), std::move(W));
}

// ── Engine adapters ───────────────────────────────────────────────────
namespace detail {

void butter_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("butter: requires at least 2 arguments",
                     0, 0, "butter", "", "m:butter:nargin");
    const int N = static_cast<int>(args[0].toScalar());
    const double Wn = args[1].toScalar();
    std::string type = "low";
    if (args.size() >= 3 && args[2].isChar())
        type = args[2].toString();

    auto [bv, av] = butter(ctx.engine->allocator(), N, Wn, type);
    outs[0] = std::move(bv);
    if (nargout > 1)
        outs[1] = std::move(av);
}

void fir1_reg(Span<const MValue> args, size_t /*nargout*/, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("fir1: requires at least 2 arguments",
                     0, 0, "fir1", "", "m:fir1:nargin");
    const int N = static_cast<int>(args[0].toScalar());
    const double Wn = args[1].toScalar();
    std::string type = "low";
    if (args.size() >= 3 && args[2].isChar())
        type = args[2].toString();

    outs[0] = fir1(ctx.engine->allocator(), N, Wn, type);
}

void freqz_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("freqz: requires at least 2 arguments",
                     0, 0, "freqz", "", "m:freqz:nargin");
    const size_t npts = (args.size() >= 3) ? static_cast<size_t>(args[2].toScalar()) : 512;

    auto [H, W] = freqz(ctx.engine->allocator(), args[0], args[1], npts);
    outs[0] = std::move(H);
    if (nargout > 1)
        outs[1] = std::move(W);
}

void phasez_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("phasez: requires at least 2 arguments",
                     0, 0, "phasez", "", "m:phasez:nargin");
    const size_t npts = (args.size() >= 3) ? static_cast<size_t>(args[2].toScalar()) : 512;
    auto [phi, W] = phasez(ctx.engine->allocator(), args[0], args[1], npts);
    outs[0] = std::move(phi);
    if (nargout > 1) outs[1] = std::move(W);
}

void grpdelay_reg(Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw MError("grpdelay: requires at least 2 arguments",
                     0, 0, "grpdelay", "", "m:grpdelay:nargin");
    const size_t npts = (args.size() >= 3) ? static_cast<size_t>(args[2].toScalar()) : 512;
    auto [gd, W] = grpdelay(ctx.engine->allocator(), args[0], args[1], npts);
    outs[0] = std::move(gd);
    if (nargout > 1) outs[1] = std::move(W);
}

} // namespace detail

} // namespace numkit::m::dsp
