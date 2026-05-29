// libs/control/src/discretize/discretize.cpp
//
// c2d / d2c — sample-time conversion. Uses the shared numerical
// kernels (matrix exponential via [6/6] Padé with scaling/squaring,
// Van Loan augmented-matrix ZOH discretiser) from
// libs/control/internal/numerics — same pair the cycle-34 step
// response already validated. (Cycle 44 DRY: this file used to
// inline its own copies of expm + LU.)

#include <numkit/control/discretize/discretize.hpp>
#include <numkit/control/lti/lti.hpp>
#include <numkit/control/conversion/conversion.hpp>
#include <numkit/control/internal/numerics.hpp>

#include <numkit/builtin/math/poly/polynomials.hpp>
#include <numkit/signal/convolution/convolution.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace numkit::control {

namespace {

using Mat = internal::Mat;
using Vec = internal::Vec;
using internal::solveInPlace;
using internal::expm;

Mat zerosM(size_t r, size_t c) { return Mat(r * c, 0.0); }
Mat eyeM(size_t n) { Mat I(n * n, 0.0); for (size_t i = 0; i < n; ++i) I[i * n + i] = 1.0; return I; }

bool hasKind(const Value &sys, const char *want) {
    if (!sys.isStruct() || !sys.hasField("kind")) return false;
    return sys.field("kind").toString() == want;
}

double sampleTime(const Value &sys) {
    if (sys.isStruct() && sys.hasField("Ts"))
        return sys.field("Ts").toScalar();
    return 0.0;
}

Vec coeffsReal(const Value &v) {
    Vec out(v.numel());
    for (size_t i = 0; i < v.numel(); ++i) out[i] = v.elemAsDouble(i);
    return out;
}

Value rowOfDoubles(const Vec &v, std::pmr::memory_resource *mr) {
    Value r = Value::matrix(1, v.size(), ValueType::DOUBLE, mr);
    if (!v.empty()) std::copy(v.begin(), v.end(), r.doubleDataMut());
    return r;
}

Value matFromVec(size_t r, size_t c, const Vec &v, std::pmr::memory_resource *mr) {
    Value m = Value::matrix(r, c, ValueType::DOUBLE, mr);
    if (!v.empty()) std::copy(v.begin(), v.end(), m.doubleDataMut());
    return m;
}

// Pull (A, B, C, D) from any LTI form (continuous or discrete).
struct SS {
    size_t n;
    Mat A;
    Vec B;
    Vec C;
    double D;
    double Ts;
};

SS toSSiso(const Value &sys, std::pmr::memory_resource *mr) {
    Value Av, Bv, Cv, Dv;
    double Ts = sampleTime(sys);
    if (hasKind(sys, "ss")) {
        Av = sys.field("A"); Bv = sys.field("B");
        Cv = sys.field("C"); Dv = sys.field("D");
    } else if (hasKind(sys, "tf")) {
        auto ss = tf2ss(sys.field("num"), sys.field("den"), mr);
        Av = std::move(ss.A); Bv = std::move(ss.B);
        Cv = std::move(ss.C); Dv = std::move(ss.D);
    } else if (hasKind(sys, "zpk")) {
        auto [num, den] = zp2tf(sys.field("z"), sys.field("p"), sys.field("k"), mr);
        auto ss = tf2ss(num, den, mr);
        Av = std::move(ss.A); Bv = std::move(ss.B);
        Cv = std::move(ss.C); Dv = std::move(ss.D);
    } else {
        throw Error("c2d/d2c: expected an LTI struct (tf/zpk/ss)",
                    0, 0, "discretize", "", "numkit:control:kind");
    }
    SS s;
    s.n = Av.dims().rows();
    s.Ts = Ts;
    s.A.resize(s.n * s.n);
    for (size_t i = 0; i < s.n * s.n; ++i) s.A[i] = Av.elemAsDouble(i);
    s.B.resize(s.n);
    for (size_t i = 0; i < s.n; ++i) s.B[i] = Bv.elemAsDouble(i);
    s.C.resize(s.n);
    for (size_t i = 0; i < s.n; ++i) s.C[i] = Cv.elemAsDouble(i * 1 + 0);
    s.D = (Dv.numel() == 0) ? 0.0 : Dv.toScalar();
    return s;
}

void zohDiscretise(const Mat &A, const Vec &B, size_t n, double Ts,
                   Mat &Ad, Vec &Bd)
{
    const size_t m = n + 1;
    Mat M = zerosM(m, m);
    for (size_t j = 0; j < n; ++j)
        for (size_t i = 0; i < n; ++i)
            M[j * m + i] = A[j * n + i] * Ts;
    for (size_t i = 0; i < n; ++i)
        M[n * m + i] = B[i] * Ts;
    Mat E = expm(M, m);
    Ad.assign(n * n, 0.0);
    Bd.assign(n, 0.0);
    for (size_t j = 0; j < n; ++j)
        for (size_t i = 0; i < n; ++i)
            Ad[j * n + i] = E[j * m + i];
    for (size_t i = 0; i < n; ++i) Bd[i] = E[n * m + i];
}

// Tustin (bilinear): A_d = (I + A·Ts/2)·(I − A·Ts/2)^-1
//                    B_d = (I − A·Ts/2)^-1 · B · √Ts (scaling factor
//                                                    is √Ts so that
//                                                    DC gain matches)
// MATLAB convention: A_d = inv(I − A·Ts/2)·(I + A·Ts/2)
//                    B_d = √Ts · inv(I − A·Ts/2) · B
//                    C_d = √Ts · C · inv(I − A·Ts/2)
//                    D_d = D + (Ts/2) · C · inv(I − A·Ts/2) · B
// We use the standard (non-frequency-prewarped) form.
void tustinDiscretise(const Mat &A, const Vec &B, const Vec &C, double D,
                      size_t n, double Ts,
                      Mat &Ad, Vec &Bd, Vec &Cd, double &Dd)
{
    // Build M_minus = I − A·Ts/2  and  M_plus = I + A·Ts/2.
    Mat Mminus = eyeM(n), Mplus = eyeM(n);
    for (size_t i = 0; i < n * n; ++i) Mminus[i] -= A[i] * (Ts / 2.0);
    for (size_t i = 0; i < n * n; ++i) Mplus[i]  += A[i] * (Ts / 2.0);

    // Solve M_minus · X = M_plus   ⇒  X = inv(M_minus)·M_plus.
    Mat Mc = Mminus;
    Mat X  = Mplus;
    if (!solveInPlace(Mc, X, n, n))
        throw Error("c2d (tustin): I − A·Ts/2 is singular",
                    0, 0, "c2d", "", "numkit:c2d:singular");
    Ad = X;

    // B_d = √Ts · inv(M_minus) · B.
    Mat Mc2 = Mminus;
    Mat Bm(n, 0.0);
    for (size_t i = 0; i < n; ++i) Bm[i] = B[i];
    if (!solveInPlace(Mc2, Bm, n, 1))
        throw Error("c2d (tustin): I − A·Ts/2 is singular",
                    0, 0, "c2d", "", "numkit:c2d:singular");
    Bd.assign(n, 0.0);
    const double sTs = std::sqrt(Ts);
    for (size_t i = 0; i < n; ++i) Bd[i] = sTs * Bm[i];

    // C_d = √Ts · C · inv(M_minus). Compute via inv(M_minus)^T·Cᵀ.
    Mat McT(n * n, 0.0);
    for (size_t j = 0; j < n; ++j)
        for (size_t i = 0; i < n; ++i)
            McT[i * n + j] = Mminus[j * n + i];   // transpose
    Mat Cm(n, 0.0);
    for (size_t i = 0; i < n; ++i) Cm[i] = C[i];
    if (!solveInPlace(McT, Cm, n, 1))
        throw Error("c2d (tustin): I − A·Ts/2 is singular",
                    0, 0, "c2d", "", "numkit:c2d:singular");
    Cd.assign(n, 0.0);
    for (size_t i = 0; i < n; ++i) Cd[i] = sTs * Cm[i];

    // D_d = D + (Ts/2) · C · inv(M_minus) · B = D + (Ts/2) · Cm·B (with Cm
    // computed against the original B, not the scaled one).
    // Solve M_minus · y = B for y, then D_d = D + (Ts/2)·C·y.
    Mat Mc3 = Mminus;
    Mat yvec(n, 0.0);
    for (size_t i = 0; i < n; ++i) yvec[i] = B[i];
    if (!solveInPlace(Mc3, yvec, n, 1))
        throw Error("c2d (tustin): I − A·Ts/2 is singular",
                    0, 0, "c2d", "", "numkit:c2d:singular");
    double cy = 0.0;
    for (size_t i = 0; i < n; ++i) cy += C[i] * yvec[i];
    Dd = D + (Ts / 2.0) * cy;
}

// First-order hold (triangle approximation), SISO. Van Loan augmented
// matrix exp [[A·Ts, B·Ts, 0]; [0, 0, I·Ts]; [0, 0, 0]] → blocks
// Phi, G1, G2 (the top n rows of cols n, n+1). MATLAB R2025b:
//   Ad = Phi,  Bd = G1 + Phi·G2/Ts − G2/Ts,  Cd = C,  Dd = D + C·G2/Ts.
void fohDiscretise(const Mat &A, const Vec &B, const Vec &C, double D,
                   size_t n, double Ts, Mat &Ad, Vec &Bd, Vec &Cd, double &Dd)
{
    const size_t m = n + 2;                 // augmented dim (1 input)
    Mat M = zerosM(m, m);
    for (size_t j = 0; j < n; ++j)
        for (size_t i = 0; i < n; ++i)
            M[j * m + i] = A[j * n + i] * Ts;     // A·Ts block
    for (size_t i = 0; i < n; ++i)
        M[n * m + i] = B[i] * Ts;                  // B·Ts column (col n)
    M[(n + 1) * m + n] = Ts;                       // I·Ts entry (row n, col n+1)

    Mat E = expm(M, m);
    Ad.assign(n * n, 0.0);
    for (size_t j = 0; j < n; ++j)
        for (size_t i = 0; i < n; ++i)
            Ad[j * n + i] = E[j * m + i];          // Phi
    Vec G1(n, 0.0), G2(n, 0.0);
    for (size_t i = 0; i < n; ++i) {
        G1[i] = E[n * m + i];
        G2[i] = E[(n + 1) * m + i];
    }
    Bd.assign(n, 0.0);
    for (size_t i = 0; i < n; ++i) {
        double phiG2 = 0.0;                         // (Phi·G2)[i]
        for (size_t k = 0; k < n; ++k) phiG2 += Ad[k * n + i] * G2[k];
        Bd[i] = G1[i] + phiG2 / Ts - G2[i] / Ts;
    }
    Cd = C;
    double cg2 = 0.0;
    for (size_t i = 0; i < n; ++i) cg2 += C[i] * G2[i];
    Dd = D + cg2 / Ts;
}

// Build an output struct of the requested kind from (A, B, C, D, Ts).
Value packResult(const Mat &Ad, const Vec &Bd, const Vec &Cd, double Dd, size_t n, double Ts, const std::string &origKind, std::pmr::memory_resource *mr)
{
    Value Av = matFromVec(n, n, Ad, mr);
    Value Bv = matFromVec(n, 1, Bd, mr);
    Value Cv = matFromVec(1, n, Cd, mr);
    Value Dv = Value::scalar(Dd, mr);

    if (origKind == "ss") {
        return ss(Av, Bv, Cv, Dv, Ts, mr);
    }
    // Convert (A, B, C, D) → (num, den) via libs/control's ss2tf.
    auto [numV, denV] = ss2tf(Av, Bv, Cv, Dv, /*iu=*/1, mr);
    if (origKind == "tf") {
        return tf(numV, denV, Ts, mr);
    }
    if (origKind == "zpk") {
        Value zV = builtin::roots(numV, mr);
        Value pV = builtin::roots(denV, mr);
        // Gain = num(1)/den(1) (after stripping leading zeros).
        Vec numVec = coeffsReal(numV);
        Vec denVec = coeffsReal(denV);
        size_t in = 0; while (in + 1 < numVec.size() && numVec[in] == 0.0) ++in;
        size_t id = 0; while (id + 1 < denVec.size() && denVec[id] == 0.0) ++id;
        const double k = (in < numVec.size() && id < denVec.size())
                         ? numVec[in] / denVec[id] : 0.0;
        return zpk(zV, pV, k, Ts, mr);
    }
    return ss(Av, Bv, Cv, Dv, Ts, mr);
}

} // anonymous

Value c2d(const Value &sys, double Ts, const std::string &method, std::pmr::memory_resource *mr)
{
    if (Ts <= 0.0)
        throw Error("c2d: Ts must be positive",
                    0, 0, "c2d", "", "numkit:c2d:Ts");
    SS s = toSSiso(sys, mr);
    if (s.Ts > 0.0)
        throw Error("c2d: input system is already discrete (Ts > 0)",
                    0, 0, "c2d", "", "numkit:c2d:already_discrete");

    Mat Ad; Vec Bd, Cd; double Dd;
    if (method.empty() || method == "zoh") {
        zohDiscretise(s.A, s.B, s.n, Ts, Ad, Bd);
        // ZOH leaves C and D untouched.
        Cd = s.C;
        Dd = s.D;
    } else if (method == "tustin" || method == "bilinear") {
        tustinDiscretise(s.A, s.B, s.C, s.D, s.n, Ts, Ad, Bd, Cd, Dd);
    } else if (method == "foh") {
        fohDiscretise(s.A, s.B, s.C, s.D, s.n, Ts, Ad, Bd, Cd, Dd);
    } else {
        throw Error("c2d: method must be 'zoh', 'foh', or 'tustin'",
                    0, 0, "c2d", "", "numkit:c2d:method");
    }

    const std::string origKind = sys.isStruct() && sys.hasField("kind")
                                 ? sys.field("kind").toString()
                                 : std::string("ss");
    return packResult(Ad, Bd, Cd, Dd, s.n, Ts, origKind, mr);
}

Value d2c(const Value &sys, const std::string &method, std::pmr::memory_resource *mr)
{
    SS s = toSSiso(sys, mr);
    if (s.Ts <= 0.0)
        throw Error("d2c: input system is already continuous (Ts == 0)",
                    0, 0, "d2c", "", "numkit:d2c:already_continuous");
    if (!method.empty() && method != "tustin" && method != "bilinear")
        throw Error("d2c: only 'tustin' is supported",
                    0, 0, "d2c", "", "numkit:d2c:method");
    const double Ts = s.Ts;

    // Inverse of Tustin: from
    //   A_d = (I − A·Ts/2)^-1 · (I + A·Ts/2),
    //   B_d = √Ts · (I − A·Ts/2)^-1 · B,
    //   C_d = √Ts · C · (I − A·Ts/2)^-1,
    //   D_d = D + (Ts/2) · C · (I − A·Ts/2)^-1 · B.
    // We solve for A first:  A = (2/Ts)·(A_d + I)^-1 · (A_d − I).
    Mat Ad = s.A;     // careful: SS struct used .A as the discrete A
    Mat Aplus(s.A.size(), 0.0);    // A_d + I
    Mat Aminus(s.A.size(), 0.0);   // A_d − I
    for (size_t i = 0; i < s.n * s.n; ++i) {
        Aplus[i]  = Ad[i];
        Aminus[i] = Ad[i];
    }
    for (size_t i = 0; i < s.n; ++i) {
        Aplus[i * s.n + i]  += 1.0;
        Aminus[i * s.n + i] -= 1.0;
    }
    // Solve (A_d + I) · X = (A_d − I)  ⇒  X = (A_d + I)^-1 · (A_d − I).
    Mat ApCopy = Aplus;
    Mat X = Aminus;
    if (!solveInPlace(ApCopy, X, s.n, s.n))
        throw Error("d2c (tustin): A_d + I is singular",
                    0, 0, "d2c", "", "numkit:d2c:singular");
    // Now A = (2/Ts) · X.
    Mat Ac(s.n * s.n, 0.0);
    for (size_t i = 0; i < s.n * s.n; ++i) Ac[i] = (2.0 / Ts) * X[i];

    // Reverse the B/C/D scaling. We need (I − A_c·Ts/2) and
    // (I + A_c·Ts/2). Note (I + A_c·Ts/2) = (A_d + I)/2 (verifiable
    // from the algebra) — so the easier path is solve for B from
    //   B_d = √Ts · (I − A_c·Ts/2)^-1 · B
    //       = √Ts · ((A_d + I)/2)^-1 · B    (since I − A_c·Ts/2 = (A_d+I)/2 ... NO)
    // Actually re-derive: A_d = (I − A·Ts/2)^-1 · (I + A·Ts/2). Pre-
    // multiply: (I − A·Ts/2)·A_d = I + A·Ts/2. Adding/subtracting
    // gives  A_d − I = (A·Ts/2)·(A_d + I)  and  A_d + I = 2·(I − A·Ts/2)^-1.
    // So  (I − A·Ts/2)^-1 = (A_d + I) / 2.
    // Therefore  B = (1/√Ts) · ((A_d + I) / 2)^-1 · B_d
    //              = (2/√Ts) · (A_d + I)^-1 · B_d.
    Mat ApCopy2 = Aplus;
    Vec Bvec(s.n);
    for (size_t i = 0; i < s.n; ++i) Bvec[i] = s.B[i];
    if (!solveInPlace(ApCopy2, Bvec, s.n, 1))
        throw Error("d2c (tustin): A_d + I is singular",
                    0, 0, "d2c", "", "numkit:d2c:singular");
    Vec Bc(s.n, 0.0);
    const double inv_sqrtTs = 2.0 / std::sqrt(Ts);
    for (size_t i = 0; i < s.n; ++i) Bc[i] = inv_sqrtTs * Bvec[i];

    // C: from C_d = √Ts · C · (I − A·Ts/2)^-1 = √Ts · C · (A_d + I) / 2.
    // ⇒ C = (2/√Ts) · C_d · (A_d + I)^-1.
    // We solve Yᵀ·(A_d + I)ᵀ = (C_d)ᵀ, equivalently (A_d + I)ᵀ·Yᵀ = (C_d)ᵀ.
    Mat ApT(s.n * s.n, 0.0);
    for (size_t j = 0; j < s.n; ++j)
        for (size_t i = 0; i < s.n; ++i)
            ApT[i * s.n + j] = Aplus[j * s.n + i];
    Vec Cvec(s.n);
    for (size_t i = 0; i < s.n; ++i) Cvec[i] = s.C[i];
    if (!solveInPlace(ApT, Cvec, s.n, 1))
        throw Error("d2c (tustin): A_d + I is singular",
                    0, 0, "d2c", "", "numkit:d2c:singular");
    Vec Cc(s.n, 0.0);
    for (size_t i = 0; i < s.n; ++i) Cc[i] = inv_sqrtTs * Cvec[i];

    // D = D_d − (Ts/2) · C · (I − A·Ts/2)^-1 · B
    //   = D_d − (Ts/2) · (1/Ts) · Cc · (A_d + I) · Bc
    //   ... easier: re-derive numerically: D = D_d − C_c · (A_d + I)/2 · ... no
    // Direct approach: D_d = D + (Ts/2)·C·(A_d+I)/2·B = D + (Ts/4)·C·(A_d+I)·B.
    // With C, B already recovered above, we can plug in.
    // Compute (A_d + I) · Bc.
    Vec ApBc(s.n, 0.0);
    for (size_t i = 0; i < s.n; ++i) {
        double ssum = 0.0;
        for (size_t j = 0; j < s.n; ++j)
            ssum += Aplus[j * s.n + i] * Bc[j];
        ApBc[i] = ssum;
    }
    double cAB = 0.0;
    for (size_t i = 0; i < s.n; ++i) cAB += Cc[i] * ApBc[i];
    const double Dc = s.D - (Ts / 4.0) * cAB;

    const std::string origKind = sys.isStruct() && sys.hasField("kind")
                                 ? sys.field("kind").toString()
                                 : std::string("ss");
    // Continuous → Ts = 0.
    return packResult(Ac, Bc, Cc, Dc, s.n, 0.0, origKind, mr);
}

namespace detail {

static std::string argString(const Value &v) {
    if (!v.isChar() && !v.isString())
        throw Error("control discretize: expected a string method",
                    0, 0, "discretize", "", "numkit:discretize:type");
    return v.toString();
}

void c2d_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.size() < 2)
        throw Error("c2d: requires (sys, Ts [, method])",
                    0, 0, "c2d", "", "numkit:c2d:nargin");
    std::string method;
    if (a.size() >= 3 && !a[2].isEmpty()) method = argString(a[2]);
    o[0] = c2d(a[0], a[1].toScalar(), method, c.engine->resource());
}

void d2c_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.empty())
        throw Error("d2c: requires (sys [, method])",
                    0, 0, "d2c", "", "numkit:d2c:nargin");
    std::string method;
    if (a.size() >= 2 && !a[1].isEmpty()) method = argString(a[1]);
    o[0] = d2c(a[0], method, c.engine->resource());
}

} // namespace detail

} // namespace numkit::control
