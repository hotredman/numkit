#include "MDspLibrary.hpp"

#define _USE_MATH_DEFINES
#include <cmath>
#include <complex>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::m::m {

static std::vector<Complex> butterworthPoles(int N)
{
    std::vector<Complex> poles;
    for (int k = 0; k < N; ++k) {
        double theta = M_PI * (2.0 * k + N + 1) / (2.0 * N);
        poles.emplace_back(std::cos(theta), std::sin(theta));
    }
    return poles;
}

static std::vector<double> expandPoly(const std::vector<Complex> &roots)
{
    int n = static_cast<int>(roots.size());
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

static void bilinearTransform(const std::vector<Complex> &sPoles,
                              double Wn,
                              std::vector<double> &bOut,
                              std::vector<double> &aOut)
{
    int N = static_cast<int>(sPoles.size());

    std::vector<Complex> zPoles(N);
    for (int i = 0; i < N; ++i) {
        Complex sp = sPoles[i] * Wn;
        zPoles[i] = (1.0 + sp / 2.0) / (1.0 - sp / 2.0);
    }

    std::vector<Complex> zZeros(N, Complex(-1.0, 0.0));

    aOut = expandPoly(zPoles);
    bOut = expandPoly(zZeros);

    Complex numDC(0, 0), denDC(0, 0);
    for (size_t i = 0; i < bOut.size(); ++i)
        numDC += bOut[i];
    for (size_t i = 0; i < aOut.size(); ++i)
        denDC += aOut[i];
    double dcGain = std::abs(numDC / denDC);
    if (dcGain > 0.0)
        for (auto &v : bOut)
            v /= dcGain;
}

static void lpToHp(std::vector<double> &b, std::vector<double> &a)
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
    double nyqGain = std::abs(numNyq / denNyq);
    if (nyqGain > 0.0)
        for (auto &v : b)
            v /= nyqGain;
}

void DspLibrary::registerFilterDesignFunctions(Engine &engine)
{
    // --- butter(N, Wn) / butter(N, Wn, 'high') ---
    engine.registerFunction("butter",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                if (args.size() < 2)
                                    throw std::runtime_error(
                                        "butter requires at least 2 arguments");

                                int N = static_cast<int>(args[0].toScalar());
                                double Wn = args[1].toScalar();
                                std::string type = "low";
                                if (args.size() >= 3 && args[2].isChar())
                                    type = args[2].toString();

                                if (Wn <= 0.0 || Wn >= 1.0)
                                    throw std::runtime_error("butter: Wn must be between 0 and 1");

                                double Wa = 2.0 * std::tan(M_PI * Wn / 2.0);
                                auto sPoles = butterworthPoles(N);

                                std::vector<double> b, a;
                                bilinearTransform(sPoles, Wa, b, a);

                                if (type == "high")
                                    lpToHp(b, a);

                                auto bv = MValue::matrix(1, b.size(), MType::DOUBLE, alloc);
                                auto av = MValue::matrix(1, a.size(), MType::DOUBLE, alloc);
                                for (size_t i = 0; i < b.size(); ++i)
                                    bv.doubleDataMut()[i] = b[i];
                                for (size_t i = 0; i < a.size(); ++i)
                                    av.doubleDataMut()[i] = a[i];
                                {
                                    outs[0] = bv;
                                    if (nargout > 1)
                                        outs[1] = av;
                                    return;
                                }
                            });

    // --- fir1(N, Wn) / fir1(N, Wn, 'high') ---
    engine.registerFunction("fir1",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                if (args.size() < 2)
                                    throw std::runtime_error("fir1 requires at least 2 arguments");

                                int N = static_cast<int>(args[0].toScalar());
                                double Wn = args[1].toScalar();
                                std::string type = "low";
                                if (args.size() >= 3 && args[2].isChar())
                                    type = args[2].toString();

                                size_t filtLen = N + 1;
                                double wc = M_PI * Wn;
                                double half = N / 2.0;

                                std::vector<double> h(filtLen);
                                double hSum = 0.0;

                                for (size_t i = 0; i < filtLen; ++i) {
                                    double n = i - half;
                                    double sinc = (std::abs(n) < 1e-12)
                                                      ? wc / M_PI
                                                      : std::sin(wc * n) / (M_PI * n);
                                    double win = 0.54 - 0.46 * std::cos(2.0 * M_PI * i / N);
                                    h[i] = sinc * win;
                                    hSum += h[i];
                                }

                                if (type == "low") {
                                    for (size_t i = 0; i < filtLen; ++i)
                                        h[i] /= hSum;
                                } else if (type == "high") {
                                    for (size_t i = 0; i < filtLen; ++i)
                                        h[i] /= hSum;
                                    for (size_t i = 0; i < filtLen; ++i)
                                        h[i] = -h[i];
                                    h[static_cast<size_t>(half)] += 1.0;
                                }

                                auto bv = MValue::matrix(1, filtLen, MType::DOUBLE, alloc);
                                for (size_t i = 0; i < filtLen; ++i)
                                    bv.doubleDataMut()[i] = h[i];
                                {
                                    outs[0] = bv;
                                    return;
                                }
                            });

    // --- freqz(b, a, N) ---
    engine.registerFunction("freqz",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                if (args.size() < 2)
                                    throw std::runtime_error("freqz requires at least 2 arguments");

                                auto &bv = args[0];
                                auto &av = args[1];
                                size_t npts = (args.size() >= 3)
                                                  ? static_cast<size_t>(args[2].toScalar())
                                                  : 512;

                                const double *b = bv.doubleData();
                                const double *a = av.doubleData();
                                size_t nb = bv.numel(), na = av.numel();

                                auto W = MValue::matrix(npts, 1, MType::DOUBLE, alloc);
                                auto H = MValue::complexMatrix(npts, 1, alloc);

                                for (size_t k = 0; k < npts; ++k) {
                                    double w = M_PI * k / (npts - 1);
                                    W.doubleDataMut()[k] = w;

                                    Complex ejw(std::cos(w), -std::sin(w));
                                    Complex num(0, 0), den(0, 0);
                                    Complex ejwk(1, 0);
                                    for (size_t i = 0; i < nb; ++i) {
                                        num += b[i] * ejwk;
                                        ejwk *= ejw;
                                    }
                                    ejwk = Complex(1, 0);
                                    for (size_t i = 0; i < na; ++i) {
                                        den += a[i] * ejwk;
                                        ejwk *= ejw;
                                    }
                                    H.complexDataMut()[k] = num / den;
                                }
                                {
                                    outs[0] = H;
                                    if (nargout > 1)
                                        outs[1] = W;
                                    return;
                                }
                            });
}

} // namespace numkit::m::m