// libs/comm/src/modulation/qam.cpp

#include <numkit/comm/modulation/qam.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

namespace numkit::comm {

namespace {

using Cd = std::complex<double>;

inline int to_gray(int b) { return b ^ (b >> 1); }
inline int from_gray(int g) {
    int b = g;
    while (g >>= 1) b ^= g;
    return b;
}

// Map a Gray-coded index to PAM amplitude in {-(M-1), -(M-3), …, (M-1)}.
// MATLAB convention: pammod with default 'gray' returns
//   level = 2·gray(s) - (M-1),  where gray(s) = s ⊕ (s/2).
inline double pam_amp(int s, int M, const std::string &order) {
    int k = (order == "bin") ? s : to_gray(s);
    return 2.0 * double(k) - double(M - 1);
}

inline int pam_amp_to_symbol(double amp, int M, const std::string &order) {
    int k = (int)std::lround(0.5 * (amp + (M - 1)));
    if (k < 0) k = 0;
    if (k > M - 1) k = M - 1;
    if (order == "bin") return k;
    return from_gray(k);
}

Value alloc_double_like(std::pmr::memory_resource *mr, const Value &x) {
    const auto &d = x.dims();
    if (x.isScalar()) return Value::scalar(0.0, mr);
    if (d.is3D())
        return Value::matrix3d(d.rows(), d.cols(), d.pages(), ValueType::DOUBLE, mr);
    return Value::matrix(d.rows(), d.cols(), ValueType::DOUBLE, mr);
}

Value alloc_complex_like(std::pmr::memory_resource *mr, const Value &x) {
    const auto &d = x.dims();
    if (x.isScalar()) return Value::matrix(1, 1, ValueType::COMPLEX, mr);
    if (d.is3D())
        return Value::matrix3d(d.rows(), d.cols(), d.pages(), ValueType::COMPLEX, mr);
    return Value::matrix(d.rows(), d.cols(), ValueType::COMPLEX, mr);
}

} // anonymous

Value pammod(const Value &x, int M, double ini_phase,
             const std::string &symbol_order,
             std::pmr::memory_resource *mr)
{
    if (M < 2)
        throw Error("pammod: M must be ≥ 2", 0, 0, "pammod", "",
                    "numkit:pammod:badM");
    Value out = alloc_complex_like(mr, x);   // PAM is real-valued but
    // MATLAB still returns complex when ini_phase ≠ 0; for simplicity always
    // return complex.
    const size_t N = x.numel();
    if (N == 0) return out;
    Cd *od = out.complexDataMut();
    const Cd phase = (ini_phase == 0.0) ? Cd(1.0, 0.0)
                                        : Cd(std::cos(ini_phase), std::sin(ini_phase));
    for (size_t i = 0; i < N; ++i) {
        const int s = (int)x.elemAsDouble(i);
        const double amp = pam_amp(s, M, symbol_order);
        od[i] = Cd(amp, 0.0) * phase;
    }
    return out;
}

Value pamdemod(const Value &y, int M, double ini_phase,
               const std::string &symbol_order,
               std::pmr::memory_resource *mr)
{
    if (M < 2)
        throw Error("pamdemod: M must be ≥ 2", 0, 0, "pamdemod", "",
                    "numkit:pamdemod:badM");
    Value out = alloc_double_like(mr, y);
    const size_t N = y.numel();
    if (N == 0) return out;
    double *od = out.doubleDataMut();
    const Cd inv = std::polar(1.0, -ini_phase);
    for (size_t i = 0; i < N; ++i) {
        Cd c = (y.type() == ValueType::COMPLEX)
             ? y.complexData()[i]
             : Cd(y.elemAsDouble(i), 0.0);
        c = c * inv;
        od[i] = double(pam_amp_to_symbol(c.real(), M, symbol_order));
    }
    return out;
}

// QAM: factor M into K_I × K_Q (K_I ≥ K_Q, both nearest squares).
namespace {
std::pair<int, int> qam_grid(int M) {
    int K = (int)std::round(std::sqrt(double(M)));
    while (K > 1 && M % K != 0) --K;
    return {M / K, K};   // (Iaxis, Qaxis)
}
} // anonymous

Value qammod(const Value &x, int M, const std::string &symbol_order,
             bool unit_power, std::pmr::memory_resource *mr)
{
    if (M < 4)
        throw Error("qammod: M must be ≥ 4", 0, 0, "qammod", "",
                    "numkit:qammod:badM");
    auto [KI, KQ] = qam_grid(M);

    // Pre-compute scaling for unit average power.
    double scale = 1.0;
    if (unit_power) {
        // Avg power of {-(K-1):2:(K-1)} = (K²-1)/3 per axis. So total =
        // ((KI²-1) + (KQ²-1))/3 (if axes independent). Use sqrt.
        const double pavg = (double((KI * KI - 1) + (KQ * KQ - 1))) / 3.0;
        scale = (pavg > 0.0) ? 1.0 / std::sqrt(pavg) : 1.0;
    }

    Value out = alloc_complex_like(mr, x);
    const size_t N = x.numel();
    if (N == 0) return out;
    Cd *od = out.complexDataMut();
    for (size_t i = 0; i < N; ++i) {
        const int s = (int)x.elemAsDouble(i);
        // MATLAB layout: the symbol indexes a column-major grid where the
        // column (I axis) = s / KQ and the row (Q axis) = s % KQ. Each axis
        // is Gray-coded; I increases left→right, Q DECREASES top→bottom.
        const int col = s / KQ;
        const int row = s % KQ;
        const int gcol = (symbol_order == "bin") ? col : to_gray(col);
        const int grow = (symbol_order == "bin") ? row : to_gray(row);
        const double I = (2.0 * double(gcol) - double(KI - 1)) * scale;
        const double Q = (double(KQ - 1) - 2.0 * double(grow)) * scale;
        od[i] = Cd(I, Q);
    }
    return out;
}

Value qamdemod(const Value &y, int M, const std::string &symbol_order,
               bool unit_power, std::pmr::memory_resource *mr)
{
    if (M < 4)
        throw Error("qamdemod: M must be ≥ 4", 0, 0, "qamdemod", "",
                    "numkit:qamdemod:badM");
    auto [KI, KQ] = qam_grid(M);

    double scale = 1.0;
    if (unit_power) {
        const double pavg = (double((KI * KI - 1) + (KQ * KQ - 1))) / 3.0;
        scale = (pavg > 0.0) ? 1.0 / std::sqrt(pavg) : 1.0;
    }

    Value out = alloc_double_like(mr, y);
    const size_t N = y.numel();
    if (N == 0) return out;
    double *od = out.doubleDataMut();
    for (size_t i = 0; i < N; ++i) {
        Cd c = (y.type() == ValueType::COMPLEX)
             ? y.complexData()[i]
             : Cd(y.elemAsDouble(i), 0.0);
        const double I = c.real() / scale;
        const double Q = c.imag() / scale;
        int gcol = (int)std::lround(0.5 * (I + double(KI - 1)));
        int grow = (int)std::lround(0.5 * (double(KQ - 1) - Q));   // Q decreases
        gcol = std::clamp(gcol, 0, KI - 1);
        grow = std::clamp(grow, 0, KQ - 1);
        const int col = (symbol_order == "bin") ? gcol : from_gray(gcol);
        const int row = (symbol_order == "bin") ? grow : from_gray(grow);
        od[i] = double(col * KQ + row);
    }
    return out;
}

Value modnorm(const Value &ref, const std::string &type, double target,
              std::pmr::memory_resource *mr)
{
    const size_t N = ref.numel();
    if (N == 0) return Value::scalar(1.0, mr);
    if (type == "avpow") {
        double s = 0.0;
        for (size_t i = 0; i < N; ++i) {
            if (ref.type() == ValueType::COMPLEX) {
                Cd c = ref.complexData()[i];
                s += std::norm(c);
            } else {
                const double v = ref.elemAsDouble(i);
                s += v * v;
            }
        }
        s /= double(N);
        return Value::scalar(std::sqrt(target / s), mr);
    }
    // peakpow.
    double pk = 0.0;
    for (size_t i = 0; i < N; ++i) {
        double p = 0.0;
        if (ref.type() == ValueType::COMPLEX)
            p = std::norm(ref.complexData()[i]);
        else { const double v = ref.elemAsDouble(i); p = v * v; }
        if (p > pk) pk = p;
    }
    if (pk == 0.0) return Value::scalar(1.0, mr);
    return Value::scalar(std::sqrt(target / pk), mr);
}

// ════════════════════════════════════════════════════════════════════
// Engine adapters
// ════════════════════════════════════════════════════════════════════

namespace detail {

namespace {
// MATLAB R2025b defaults differ by function: pammod/pamdemod default to
// 'bin' (binary symbol mapping); qammod/qamdemod default to 'gray'.
std::string parse_order(Span<const Value> args, size_t start, const char *dflt) {
    for (size_t i = start; i < args.size(); ++i) {
        if (args[i].isChar() || args[i].isString()) {
            auto s = args[i].toString();
            if (s == "bin" || s == "gray") return s;
        }
    }
    return dflt;
}

bool parse_unit_power(Span<const Value> args, size_t start) {
    for (size_t i = start; i + 1 < args.size(); ++i) {
        if ((args[i].isChar() || args[i].isString())
            && args[i].toString() == "UnitAveragePower")
            return args[i + 1].toScalar() != 0.0;
    }
    return false;
}
}

void pammod_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("pammod: requires (x, M[, ini_phase, symbol_order])",
                    0, 0, "pammod", "", "numkit:pammod:nargin");
    const int M = (int)args[1].toScalar();
    const double ini = (args.size() >= 3 && !args[2].isEmpty()
                        && !(args[2].isChar() || args[2].isString()))
                        ? args[2].toScalar() : 0.0;
    auto order = parse_order(args, 2, "bin");
    outs[0] = pammod(args[0], M, ini, order, ctx.engine->resource());
}

void pamdemod_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("pamdemod: requires (y, M[, ini_phase, symbol_order])",
                    0, 0, "pamdemod", "", "numkit:pamdemod:nargin");
    const int M = (int)args[1].toScalar();
    const double ini = (args.size() >= 3 && !args[2].isEmpty()
                        && !(args[2].isChar() || args[2].isString()))
                        ? args[2].toScalar() : 0.0;
    auto order = parse_order(args, 2, "bin");
    outs[0] = pamdemod(args[0], M, ini, order, ctx.engine->resource());
}

void qammod_reg(Span<const Value> args, size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("qammod: requires (x, M[, symbol_order, 'UnitAveragePower', tf])",
                    0, 0, "qammod", "", "numkit:qammod:nargin");
    const int M = (int)args[1].toScalar();
    auto order = parse_order(args, 2, "gray");
    bool up = parse_unit_power(args, 2);
    outs[0] = qammod(args[0], M, order, up, ctx.engine->resource());
}

void qamdemod_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("qamdemod: requires (y, M[, symbol_order, 'UnitAveragePower', tf])",
                    0, 0, "qamdemod", "", "numkit:qamdemod:nargin");
    const int M = (int)args[1].toScalar();
    auto order = parse_order(args, 2, "gray");
    bool up = parse_unit_power(args, 2);
    outs[0] = qamdemod(args[0], M, order, up, ctx.engine->resource());
}

void modnorm_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("modnorm: requires (ref, type, target)",
                    0, 0, "modnorm", "", "numkit:modnorm:nargin");
    std::string type = "avpow";
    if (args[1].isChar() || args[1].isString()) type = args[1].toString();
    const double target = args[2].toScalar();
    outs[0] = modnorm(args[0], type, target, ctx.engine->resource());
}

} // namespace detail
} // namespace numkit::comm
