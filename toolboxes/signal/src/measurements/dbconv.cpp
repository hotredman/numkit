// toolboxes/signal/src/measurements/dbconv.cpp
//
// db / db2mag / mag2db / pow2db / db2pow.

#include <numkit/signal/measurements/dbconv.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <numkit/ops/helpers.hpp>  // createLike (toolboxes/builtin/src/)

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
Value db(const Value &x, const std::string &signalType, std::pmr::memory_resource *mr)
{
    const std::string mode = toLower(signalType);
    double scale;
    if (mode == "voltage" || mode.empty())
        scale = 20.0;
    else if (mode == "power")
        scale = 10.0;
    else
        throw Error("db: signalType must be 'voltage' or 'power'",
                     0, 0, "db", "", "numkit:db:badType");
    auto out = createLike(x, ValueType::DOUBLE, mr);
    forEachAsMag(x, out.doubleDataMut(),
                 [scale](double v) { return scale * std::log10(v); });
    return out;
}

// ── db2mag ─────────────────────────────────────────────────────────────
Value db2mag(const Value &d, std::pmr::memory_resource *mr)
{
    auto out = createLike(d, ValueType::DOUBLE, mr);
    forEachAsMag(d, out.doubleDataMut(),
                 [](double v) { return std::pow(10.0, v / 20.0); });
    return out;
}

// ── mag2db ─────────────────────────────────────────────────────────────
Value mag2db(const Value &x, std::pmr::memory_resource *mr)
{
    auto out = createLike(x, ValueType::DOUBLE, mr);
    forEachAsMag(x, out.doubleDataMut(),
                 [](double v) { return 20.0 * std::log10(v); });
    return out;
}

// ── db2pow ─────────────────────────────────────────────────────────────
Value db2pow(const Value &d, std::pmr::memory_resource *mr)
{
    auto out = createLike(d, ValueType::DOUBLE, mr);
    forEachAsMag(d, out.doubleDataMut(),
                 [](double v) { return std::pow(10.0, v / 10.0); });
    return out;
}

// ── pow2db ─────────────────────────────────────────────────────────────
Value pow2db(const Value &p, std::pmr::memory_resource *mr)
{
    auto out = createLike(p, ValueType::DOUBLE, mr);
    forEachAsMag(p, out.doubleDataMut(),
                 [](double v) { return 10.0 * std::log10(v); });
    return out;
}

} // namespace numkit::signal
