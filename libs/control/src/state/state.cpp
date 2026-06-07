// libs/control/src/state/state.cpp
//
// ctrb / obsv. Pure matrix algebra — no expm, no eigensolver. The
// 2-arg forms accept (A, B) / (A, C) coefficient pairs directly, so
// they work without an LTI struct; the 1-arg forms peel A,B,C out
// of the cycle-31 sys struct (going through ss form for tf/zpk).

#include <numkit/control/state/state.hpp>
#include <numkit/control/conversion/conversion.hpp>

// Compute-only TU: Value substrate + Error, no engine. The ctrb / obsv
// builtins (CallContext wrappers) live in state/state_reg.cpp.
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
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

} // namespace numkit::control
