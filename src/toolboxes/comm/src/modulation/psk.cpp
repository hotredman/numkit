// toolboxes/comm/src/modulation/psk.cpp

#include <numkit/comm/modulation/psk.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <string>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::comm {

namespace {

using Cd = std::complex<double>;

// Gray code encoding: g = b ⊕ (b >> 1).
inline int to_gray(int b) { return b ^ (b >> 1); }

// Gray → binary.
inline int from_gray(int g) {
    int b = g;
    while (g >>= 1) b ^= g;
    return b;
}

// Map an integer symbol s ∈ 0..M-1 to a phase index in 0..M-1 according
// to the chosen ordering. For "gray" the natural index k is replaced
// by gray⁻¹(k); for "bin" (binary), it's identity.
inline int symbol_to_phase_idx(int s, int M, const std::string &order) {
    if (order == "bin") return s;
    // Default: "gray". MATLAB convention: phase 2π·k/M corresponds to
    // symbol s where Gray(s) = k. Inverse: s = Gray⁻¹(k) → k = Gray(s).
    return to_gray(s) % M;
}

inline int phase_idx_to_symbol(int k, int M, const std::string &order) {
    if (order == "bin") return k;
    return from_gray(k) % M;
}

Value alloc_complex_like(std::pmr::memory_resource *mr, const Value &x) {
    const auto &d = x.dims();
    if (x.isScalar()) return Value::matrix(1, 1, ValueType::COMPLEX, mr);
    if (d.is3D())
        return Value::matrix3d(d.rows(), d.cols(), d.pages(), ValueType::COMPLEX, mr);
    return Value::matrix(d.rows(), d.cols(), ValueType::COMPLEX, mr);
}

Value alloc_double_like(std::pmr::memory_resource *mr, const Value &x) {
    const auto &d = x.dims();
    if (x.isScalar()) return Value::scalar(0.0, mr);
    if (d.is3D())
        return Value::matrix3d(d.rows(), d.cols(), d.pages(), ValueType::DOUBLE, mr);
    return Value::matrix(d.rows(), d.cols(), ValueType::DOUBLE, mr);
}

} // anonymous

Value pskmod(const Value &x, int M, double ini_phase,
             const std::string &symbol_order,
             std::pmr::memory_resource *mr)
{
    if (M < 2)
        throw Error("pskmod: M must be ≥ 2", 0, 0, "pskmod", "",
                    "numkit:pskmod:badM");
    Value out = alloc_complex_like(mr, x);
    const size_t N = x.numel();
    if (N == 0) return out;
    Cd *od = out.complexDataMut();
    const double dphi = 2.0 * M_PI / double(M);
    for (size_t i = 0; i < N; ++i) {
        const int s = (int)x.elemAsDouble(i);
        const int k = symbol_to_phase_idx(s, M, symbol_order);
        const double theta = dphi * k + ini_phase;
        od[i] = Cd(std::cos(theta), std::sin(theta));
    }
    return out;
}

Value pskdemod(const Value &y, int M, double ini_phase,
               const std::string &symbol_order,
               std::pmr::memory_resource *mr)
{
    if (M < 2)
        throw Error("pskdemod: M must be ≥ 2", 0, 0, "pskdemod", "",
                    "numkit:pskdemod:badM");
    Value out = alloc_double_like(mr, y);
    const size_t N = y.numel();
    if (N == 0) return out;
    double *od = out.doubleDataMut();
    const double dphi = 2.0 * M_PI / double(M);
    for (size_t i = 0; i < N; ++i) {
        Cd c;
        if (y.type() == ValueType::COMPLEX) c = y.complexData()[i];
        else c = Cd(y.elemAsDouble(i), 0.0);
        // Subtract initial phase, find nearest constellation point.
        double angle = std::arg(c) - ini_phase;
        int k = (int)std::lround(angle / dphi);
        // Wrap to 0..M-1.
        k = ((k % M) + M) % M;
        od[i] = double(phase_idx_to_symbol(k, M, symbol_order));
    }
    return out;
}

Value dpskmod(const Value &x, int M, double phase_rot,
              const std::string &symbol_order,
              std::pmr::memory_resource *mr)
{
    if (M < 2)
        throw Error("dpskmod: M must be ≥ 2", 0, 0, "dpskmod", "",
                    "numkit:dpskmod:badM");
    Value out = alloc_complex_like(mr, x);
    const size_t N = x.numel();
    if (N == 0) return out;
    Cd *od = out.complexDataMut();
    const double dphi = 2.0 * M_PI / double(M);
    double phase = phase_rot;  // initial reference
    for (size_t i = 0; i < N; ++i) {
        const int s = (int)x.elemAsDouble(i);
        const int k = symbol_to_phase_idx(s, M, symbol_order);
        phase += dphi * k;
        od[i] = Cd(std::cos(phase), std::sin(phase));
    }
    return out;
}

Value dpskdemod(const Value &y, int M, double phase_rot,
                const std::string &symbol_order,
                std::pmr::memory_resource *mr)
{
    if (M < 2)
        throw Error("dpskdemod: M must be ≥ 2", 0, 0, "dpskdemod", "",
                    "numkit:dpskdemod:badM");
    Value out = alloc_double_like(mr, y);
    const size_t N = y.numel();
    if (N == 0) return out;
    double *od = out.doubleDataMut();
    const double dphi = 2.0 * M_PI / double(M);
    double prev_phase = phase_rot;
    for (size_t i = 0; i < N; ++i) {
        Cd c;
        if (y.type() == ValueType::COMPLEX) c = y.complexData()[i];
        else c = Cd(y.elemAsDouble(i), 0.0);
        const double cur = std::arg(c);
        double diff = cur - prev_phase;
        int k = (int)std::lround(diff / dphi);
        k = ((k % M) + M) % M;
        od[i] = double(phase_idx_to_symbol(k, M, symbol_order));
        prev_phase = cur;
    }
    return out;
}

} // namespace numkit::comm
