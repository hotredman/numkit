// toolboxes/control/src/state/state.cpp
//
// ctrb / obsv. Pure matrix algebra — no expm, no eigensolver. The
// 2-arg forms accept (A, B) / (A, C) coefficient pairs directly, so
// they work without an LTI struct; the 1-arg forms peel A,B,C out
// of the cycle-31 sys struct (going through ss form for tf/zpk).

#include <numkit/control/state/state.hpp>
#include <numkit/control/conversion/conversion.hpp>
#include <numkit/control/lyapunov/lyapunov.hpp>   // gram delegates to lyap / dlyap

// Compute-only TU: Value substrate + Error, no engine. The ctrb / obsv
// builtins (CallContext wrappers) live in state/state_reg.cpp.
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <vector>

namespace numkit::control {

namespace {

bool hasKind(const Value &sys, const char *want) {
    if (!sys.isStruct() || !sys.hasField("kind")) return false;
    return sys.field("kind").toString() == want;
}

// (A, B, C) extraction: returns A,B,C as plain Values regardless of
// the input form. The shapes are ss-canonical:
//   A : n×n, B : n×m, C : p×n.
struct ABC { Value A, B, C, D; };
ABC pullABC(const Value &sys, std::pmr::memory_resource *mr) {
    ABC out;
    if (hasKind(sys, "ss")) {
        out.A = sys.field("A"); out.B = sys.field("B");
        out.C = sys.field("C"); out.D = sys.field("D");
    } else if (hasKind(sys, "tf")) {
        auto ss = tf2ss(sys.field("num"), sys.field("den"), mr);
        out.A = std::move(ss.A); out.B = std::move(ss.B);
        out.C = std::move(ss.C); out.D = std::move(ss.D);
    } else if (hasKind(sys, "zpk")) {
        auto [num, den] = zp2tf(sys.field("z"), sys.field("p"), sys.field("k"), mr);
        auto ss = tf2ss(num, den, mr);
        out.A = std::move(ss.A); out.B = std::move(ss.B);
        out.C = std::move(ss.C); out.D = std::move(ss.D);
    } else {
        throw Error("ctrb/obsv: expected an LTI struct (tf/zpk/ss)",
                    0, 0, "control", "", "numkit:control:kind");
    }
    return out;
}

// Read a matrix into a column-major double buffer.
std::vector<double> readMat(const Value &v, size_t r, size_t c) {
    std::vector<double> M(r * c, 0.0);
    for (size_t i = 0; i < r * c; ++i) M[i] = v.elemAsDouble(i);
    return M;
}

Value matFromVec(size_t r, size_t c, const std::vector<double> &v, std::pmr::memory_resource *mr) {
    Value m = Value::matrix(r, c, ValueType::DOUBLE, mr);
    if (!v.empty()) std::copy(v.begin(), v.end(), m.doubleDataMut());
    return m;
}

} // anonymous

Value ctrb_AB(const Value &Av, const Value &Bv, std::pmr::memory_resource *mr)
{
    const size_t n = Av.dims().rows();
    if (Av.dims().cols() != n)
        throw Error("ctrb: A must be square",
                    0, 0, "ctrb", "", "numkit:ctrb:A");
    const size_t m = Bv.dims().cols();
    if (Bv.dims().rows() != n)
        throw Error("ctrb: B must have the same row count as A",
                    0, 0, "ctrb", "", "numkit:ctrb:B");

    auto A = readMat(Av, n, n);
    auto B = readMat(Bv, n, m);

    // Output Co is n × (n·m), column-major. Block k (k=0..n-1) holds
    // A^k · B at columns [k·m … k·m + m − 1].
    std::vector<double> Co(n * (n * m), 0.0);
    // Block 0 = B.
    for (size_t j = 0; j < m; ++j)
        for (size_t i = 0; i < n; ++i)
            Co[j * n + i] = B[j * n + i];
    // Blocks 1..n-1 by repeated multiplication: prev = A^k · B,
    // next = A · prev.
    std::vector<double> prev = B;
    for (size_t k = 1; k < n; ++k) {
        std::vector<double> next(n * m, 0.0);
        for (size_t j = 0; j < m; ++j)
            for (size_t i = 0; i < n; ++i) {
                double s = 0.0;
                for (size_t l = 0; l < n; ++l)
                    s += A[l * n + i] * prev[j * n + l];
                next[j * n + i] = s;
            }
        // Place next into block k of Co.
        const size_t colOffset = k * m;
        for (size_t j = 0; j < m; ++j)
            for (size_t i = 0; i < n; ++i)
                Co[(colOffset + j) * n + i] = next[j * n + i];
        prev = std::move(next);
    }
    return matFromVec(n, n * m, Co, mr);
}

Value obsv_AC(const Value &Av, const Value &Cv, std::pmr::memory_resource *mr)
{
    const size_t n = Av.dims().rows();
    if (Av.dims().cols() != n)
        throw Error("obsv: A must be square",
                    0, 0, "obsv", "", "numkit:obsv:A");
    const size_t p = Cv.dims().rows();
    if (Cv.dims().cols() != n)
        throw Error("obsv: C must have the same column count as A",
                    0, 0, "obsv", "", "numkit:obsv:C");

    auto A = readMat(Av, n, n);
    auto C = readMat(Cv, p, n);

    // Output Ob is (n·p) × n, column-major. Block k (k=0..n-1) holds
    // C · A^k at rows [k·p … k·p + p − 1].
    std::vector<double> Ob((n * p) * n, 0.0);
    // Block 0 = C.
    for (size_t j = 0; j < n; ++j)
        for (size_t i = 0; i < p; ++i)
            Ob[j * (n * p) + i] = C[j * p + i];
    // Walk k = 1..n-1: next = prev · A.
    std::vector<double> prev = C;
    for (size_t k = 1; k < n; ++k) {
        std::vector<double> next(p * n, 0.0);
        for (size_t j = 0; j < n; ++j)
            for (size_t i = 0; i < p; ++i) {
                double s = 0.0;
                for (size_t l = 0; l < n; ++l)
                    s += prev[l * p + i] * A[j * n + l];
                next[j * p + i] = s;
            }
        const size_t rowOffset = k * p;
        for (size_t j = 0; j < n; ++j)
            for (size_t i = 0; i < p; ++i)
                Ob[j * (n * p) + (rowOffset + i)] = next[j * p + i];
        prev = std::move(next);
    }
    return matFromVec(n * p, n, Ob, mr);
}

Value ctrb_sys(const Value &sys, std::pmr::memory_resource *mr) {
    auto abc = pullABC(sys, mr);
    return ctrb_AB(abc.A, abc.B, mr);
}

Value obsv_sys(const Value &sys, std::pmr::memory_resource *mr) {
    auto abc = pullABC(sys, mr);
    return obsv_AC(abc.A, abc.C, mr);
}

Value gram(const Value &sys, const std::string &type, std::pmr::memory_resource *mr)
{
    auto abc = pullABC(sys, mr);
    const size_t n = abc.A.dims().rows();
    const bool discrete =
        (sys.isStruct() && sys.hasField("Ts") && sys.field("Ts").toScalar() != 0.0);

    const char t = type.empty()
                 ? 'c'
                 : static_cast<char>(std::tolower(static_cast<unsigned char>(type[0])));
    if (t != 'c' && t != 'o')
        throw Error("gram: type must be 'c' (controllability) or 'o' (observability)",
                    0, 0, "gram", "", "numkit:gram:type");

    // gram solves a Lyapunov equation; the controllability / observability
    // gramian differs only in which (A, Q) pair is fed to lyap / dlyap:
    //   'c': A·Wc + Wc·Aᵀ + B·Bᵀ = 0   → lyap(A,  B·Bᵀ)
    //   'o': Aᵀ·Wo + Wo·A + Cᵀ·C = 0   → lyap(Aᵀ, Cᵀ·C)
    auto A = readMat(abc.A, n, n);
    std::vector<double> Q(n * n, 0.0);
    Value Ause;
    if (t == 'c') {
        const size_t m = abc.B.dims().cols();
        auto B = readMat(abc.B, n, m);
        for (size_t j = 0; j < n; ++j)
            for (size_t i = 0; i < n; ++i) {
                double s = 0.0;
                for (size_t k = 0; k < m; ++k) s += B[k * n + i] * B[k * n + j];  // (B·Bᵀ)[i,j]
                Q[j * n + i] = s;
            }
        Ause = abc.A;
    } else {
        const size_t p = abc.C.dims().rows();
        auto C = readMat(abc.C, p, n);   // p×n column-major: C[i,j] = C[j*p + i]
        for (size_t j = 0; j < n; ++j)
            for (size_t i = 0; i < n; ++i) {
                double s = 0.0;
                for (size_t k = 0; k < p; ++k) s += C[i * p + k] * C[j * p + k];  // (Cᵀ·C)[i,j]
                Q[j * n + i] = s;
            }
        std::vector<double> At(n * n, 0.0);
        for (size_t j = 0; j < n; ++j)
            for (size_t i = 0; i < n; ++i) At[j * n + i] = A[i * n + j];   // Aᵀ
        Ause = matFromVec(n, n, At, mr);
    }
    Value Qv = matFromVec(n, n, Q, mr);
    return discrete ? dlyap(Ause, Qv, mr) : lyap(Ause, Qv, mr);
}

CovarResult covar(const Value &sys, const Value &Wv, std::pmr::memory_resource *mr)
{
    auto abc = pullABC(sys, mr);
    const size_t n = abc.A.dims().rows();
    const size_t m = abc.B.dims().cols();
    const size_t p = abc.C.dims().rows();
    const bool discrete =
        (sys.isStruct() && sys.hasField("Ts") && sys.field("Ts").toScalar() != 0.0);

    auto B = readMat(abc.B, n, m);
    auto C = readMat(abc.C, p, n);
    auto D = readMat(abc.D, p, m);

    // Noise intensity: scalar W → W·I_m, or a full m×m matrix.
    const bool wScalar = (Wv.numel() == 1);
    const std::vector<double> W = wScalar ? std::vector<double>{} : readMat(Wv, m, m);
    const double wval = wScalar ? Wv.toScalar() : 0.0;
    auto Wij = [&](size_t i, size_t j) -> double {
        return wScalar ? (i == j ? wval : 0.0) : W[j * m + i];
    };

    // State covariance Q solves the gramian Lyapunov equation with B·W·Bᵀ.
    std::vector<double> BWBt(n * n, 0.0);
    for (size_t a = 0; a < n; ++a)
        for (size_t b = 0; b < n; ++b) {
            double s = 0.0;
            for (size_t i = 0; i < m; ++i)
                for (size_t j = 0; j < m; ++j)
                    s += B[i * n + a] * Wij(i, j) * B[j * n + b];   // B[a,i]·W[i,j]·B[b,j]
            BWBt[b * n + a] = s;
        }
    Value BWBtV = matFromVec(n, n, BWBt, mr);
    Value Q = discrete ? dlyap(abc.A, BWBtV, mr) : lyap(abc.A, BWBtV, mr);

    // Output covariance P = C·Q·Cᵀ (+ D·W·Dᵀ discrete; ∞ if continuous & D≠0).
    auto Qm = readMat(Q, n, n);
    std::vector<double> P(p * p, 0.0);
    for (size_t a = 0; a < p; ++a)
        for (size_t b = 0; b < p; ++b) {
            double s = 0.0;
            for (size_t i = 0; i < n; ++i)
                for (size_t j = 0; j < n; ++j)
                    s += C[i * p + a] * Qm[j * n + i] * C[j * p + b];   // C[a,i]·Q[i,j]·C[b,j]
            P[b * p + a] = s;
        }
    bool dNonzero = false;
    for (double d : D) if (d != 0.0) { dNonzero = true; break; }
    if (discrete) {
        for (size_t a = 0; a < p; ++a)
            for (size_t b = 0; b < p; ++b) {
                double s = 0.0;
                for (size_t i = 0; i < m; ++i)
                    for (size_t j = 0; j < m; ++j)
                        s += D[i * p + a] * Wij(i, j) * D[j * p + b];   // D·W·Dᵀ
                P[b * p + a] += s;
            }
    } else if (dNonzero) {
        std::fill(P.begin(), P.end(), std::numeric_limits<double>::infinity());
    }

    CovarResult out;
    out.P = matFromVec(p, p, P, mr);
    out.Q = std::move(Q);
    return out;
}

} // namespace numkit::control
