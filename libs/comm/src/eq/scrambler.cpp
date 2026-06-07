// libs/comm/src/eq/scrambler.cpp
//
// Multiplicative scrambler / descrambler. Both share the same
// polynomial and initial state; the descrambler is the algebraic
// inverse of the scrambler.
//
// Multiplicative scrambler (per-bit):
//   fb = XOR over i=1..n where g_i = 1 of state[i-1]
//   y[k] = x[k] XOR fb
//   shift state right by 1, store y[k] at state[0]
//
// Multiplicative descrambler:
//   fb = XOR over i=1..n where g_i = 1 of state[i-1]
//   x[k] = y[k] XOR fb
//   shift state right by 1, store y[k] at state[0]   (NOT x[k])
//
// Note both sides clock in the *channel* bit y[k]: this is what makes
// the descrambler self-synchronizing — losing or flipping a few
// channel bits eventually clears out of the register.

#include <numkit/comm/eq/scrambler.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <string>
#include <vector>

namespace numkit::comm {

namespace {

std::vector<int> readBitVec(const Value &v, const char *name) {
    const size_t N = v.numel();
    std::vector<int> out(N);
    for (size_t i = 0; i < N; ++i) {
        const double x = v.elemAsDouble(i);
        const int b = (x != 0.0) ? 1 : 0;
        out[i] = b;
    }
    return out;
}

void validatePoly(const std::vector<int> &poly, const char *name) {
    if (poly.size() < 2)
        throw Error(std::string(name) +
                    ": polynomial must have at least 2 coefficients (degree ≥ 1)",
                    0, 0, name, "", "numkit:scrambler:poly");
    if (poly[0] == 0)
        throw Error(std::string(name) +
                    ": polynomial constant term g_0 must be non-zero",
                    0, 0, name, "", "numkit:scrambler:poly");
}

Value packDoubles(std::pmr::memory_resource *mr,
                  const std::vector<int> &bits)
{
    Value out = Value::matrix(bits.size(), 1, ValueType::DOUBLE, mr);
    if (!bits.empty()) {
        double *od = out.doubleDataMut();
        for (size_t i = 0; i < bits.size(); ++i) od[i] = double(bits[i]);
    }
    return out;
}

} // anonymous

Value scrambler(const Value &x, const Value &poly,
                const Value &initState,
                std::pmr::memory_resource *mr)
{
    auto xBits   = readBitVec(x, "scrambler");
    auto polyVec = readBitVec(poly, "scrambler");
    auto state   = readBitVec(initState, "scrambler");
    validatePoly(polyVec, "scrambler");
    const size_t n = polyVec.size() - 1;
    if (state.size() != n)
        throw Error("scrambler: initState length must equal polynomial order",
                    0, 0, "scrambler", "", "numkit:scrambler:state");

    std::vector<int> y(xBits.size(), 0);
    for (size_t k = 0; k < xBits.size(); ++k) {
        int fb = 0;
        for (size_t i = 1; i <= n; ++i)
            if (polyVec[i]) fb ^= state[i - 1];
        y[k] = xBits[k] ^ fb;
        // Shift register right by one position; new MSB = y[k].
        for (size_t i = n; i-- > 1;) state[i] = state[i - 1];
        state[0] = y[k];
    }
    return packDoubles(mr, y);
}

Value descrambler(const Value &y, const Value &poly,
                  const Value &initState,
                  std::pmr::memory_resource *mr)
{
    auto yBits   = readBitVec(y, "descrambler");
    auto polyVec = readBitVec(poly, "descrambler");
    auto state   = readBitVec(initState, "descrambler");
    validatePoly(polyVec, "descrambler");
    const size_t n = polyVec.size() - 1;
    if (state.size() != n)
        throw Error("descrambler: initState length must equal polynomial order",
                    0, 0, "descrambler", "", "numkit:descrambler:state");

    std::vector<int> x(yBits.size(), 0);
    for (size_t k = 0; k < yBits.size(); ++k) {
        int fb = 0;
        for (size_t i = 1; i <= n; ++i)
            if (polyVec[i]) fb ^= state[i - 1];
        x[k] = yBits[k] ^ fb;
        // Shift register; clock in the CHANNEL bit y[k] (not x[k]).
        for (size_t i = n; i-- > 1;) state[i] = state[i - 1];
        state[0] = yBits[k];
    }
    return packDoubles(mr, x);
}

} // namespace numkit::comm
