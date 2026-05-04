// libs/control/src/discretize/discretize.cpp
//
// c2d / d2c — sample-time conversion. Reuses the standard numerical
// kernels (matrix exponential via [6/6] Padé with scaling/squaring,
// Van Loan augmented-matrix ZOH discretiser) that the cycle-34 step
// response already validated. The expm + LU helpers are duplicated
// inline here for now; they're identical to the response.cpp pair
// and total ~80 LOC — small enough to live alongside until a wider
// libs/control/numerics_/ shared module shows up.

#include <numkit/control/discretize/discretize.hpp>
#include <numkit/control/lti/lti.hpp>
#include <numkit/control/conversion/conversion.hpp>

#include <numkit/builtin/math/poly/polynomials.hpp>
#include <numkit/signal/convolution/convolution.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <vector>

namespace numkit::control {

namespace {

using Mat = std::vector<double>;
using Vec = std::vector<double>;

Mat zerosM(size_t r, size_t c) { return Mat(r * c, 0.0); }
Mat eyeM(size_t n) { Mat I(n * n, 0.0); for (size_t i = 0; i < n; ++i) I[i * n + i] = 1.0; return I; }

Mat matmul(const Mat &A, size_t Ar, size_t Ac,
           const Mat &B, size_t Br, size_t Bc)
{
    (void)Br;
    Mat C(Ar * Bc, 0.0);
    for (size_t j = 0; j < Bc; ++j)
        for (size_t i = 0; i < Ar; ++i) {
            double s = 0.0;
            for (size_t k = 0; k < Ac; ++k)
                s += A[k * Ar + i] * B[j * Ac + k];
            C[j * Ar + i] = s;
        }
    return C;
}

double matInfNorm(const Mat &A, size_t n) {
    double m = 0.0;
    for (size_t i = 0; i < n; ++i) {
        double s = 0.0;
        for (size_t j = 0; j < n; ++j) s += std::abs(A[j * n + i]);
        m = std::max(m, s);
    }
    return m;
}

bool solveInPlace(Mat &A, Mat &B, size_t n, size_t nrhs)
{
    for (size_t k = 0; k < n; ++k) {
        size_t pk = k;
        double bestAbs = std::abs(A[k * n + k]);
        for (size_t i = k + 1; i < n; ++i) {
            double v = std::abs(A[k * n + i]);
            if (v > bestAbs) { bestAbs = v; pk = i; }
        }
        if (bestAbs < 1e-14) return false;
        if (pk != k) {
            for (size_t j = 0; j < n; ++j) std::swap(A[j * n + k], A[j * n + pk]);
            for (size_t j = 0; j < nrhs; ++j) std::swap(B[j * n + k], B[j * n + pk]);
        }
        const double diag = A[k * n + k];
        for (size_t i = k + 1; i < n; ++i) {
            const double f = A[k * n + i] / diag;
            A[k * n + i] = f;
            for (size_t j = k + 1; j < n; ++j)
                A[j * n + i] -= f * A[j * n + k];
            for (size_t j = 0; j < nrhs; ++j)
                B[j * n + i] -= f * B[j * n + k];
        }
    }
    for (size_t j = 0; j < nrhs; ++j) {
        for (size_t i = n; i-- > 0;) {
            double s = B[j * n + i];
            for (size_t k = i + 1; k < n; ++k) s -= A[k * n + i] * B[j * n + k];
            B[j * n + i] = s / A[i * n + i];
        }
    }
    return true;
}

Mat expm(const Mat &Ain, size_t n)
{
    if (n == 0) return Mat{};
    Mat A = Ain;
    const double normA = matInfNorm(A, n);
    int s = 0;
    if (normA > 0.5) {
        const double l2 = std::log2(normA / 0.5);
        s = static_cast<int>(std::ceil(std::max(l2, 0.0)));
    }
    if (s > 0) {
        const double scale = std::pow(0.5, s);
        for (auto &v : A) v *= scale;
    }
    // Canonical [6/6] Padé coefficients for exp(x):
    //   c_k = (12 − k)! · 6! / (12! · k! · (6 − k)!)
    static const double c[7] = {
        1.0,
        1.0/2.0,
        5.0/44.0,
        1.0/66.0,
        1.0/792.0,
        1.0/15840.0,
        1.0/665280.0
    };
    Mat I = eyeM(n);
    Mat A2 = matmul(A, n, n, A, n, n);
    Mat A3 = matmul(A2, n, n, A, n, n);
    Mat A4 = matmul(A2, n, n, A2, n, n);
    Mat A5 = matmul(A4, n, n, A, n, n);
    Mat A6 = matmul(A4, n, n, A2, n, n);
    auto axpy = [&](Mat &dst, const Mat &src, double a) {
        for (size_t i = 0; i < n * n; ++i) dst[i] += a * src[i];
    };
    Mat N = zerosM(n, n), D = zerosM(n, n);
    axpy(N, I,  c[0]); axpy(D, I,  c[0]);
    axpy(N, A,  c[1]); axpy(D, A, -c[1]);
    axpy(N, A2, c[2]); axpy(D, A2, c[2]);
    axpy(N, A3, c[3]); axpy(D, A3,-c[3]);
    axpy(N, A4, c[4]); axpy(D, A4, c[4]);
    axpy(N, A5, c[5]); axpy(D, A5,-c[5]);
    axpy(N, A6, c[6]); axpy(D, A6, c[6]);
    Mat Dcopy = D;
    Mat X = N;
    if (!solveInPlace(Dcopy, X, n, n)) {
        // Series fallback.
        Mat E = I;
        Mat term = I;
        for (int k = 1; k < 30; ++k) {
            term = matmul(term, n, n, A, n, n);
            const double inv = 1.0 / double(k);
            for (auto &v : term) v *= inv;
            for (size_t i = 0; i < n * n; ++i) E[i] += term[i];
        }
        X = E;
    }
    for (int k = 0; k < s; ++k) X = matmul(X, n, n, X, n, n);
    return X;
}

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

Value rowOfDoubles(std::pmr::memory_resource *mr, const Vec &v) {
    Value r = Value::matrix(1, v.size(), ValueType::DOUBLE, mr);
    if (!v.empty()) std::copy(v.begin(), v.end(), r.doubleDataMut());
    return r;
}

Value matFromVec(std::pmr::memory_resource *mr,
                 size_t r, size_t c, const Vec &v) {
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

SS toSSiso(std::pmr::memory_resource *mr, const Value &sys) {
    Value Av, Bv, Cv, Dv;
    double Ts = sampleTime(sys);
    if (hasKind(sys, "ss")) {
        Av = sys.field("A"); Bv = sys.field("B");
        Cv = sys.field("C"); Dv = sys.field("D");
    } else if (hasKind(sys, "tf")) {
        tf2ss(mr, sys.field("num"), sys.field("den"),
              &Av, &Bv, &Cv, &Dv);
    } else if (hasKind(sys, "zpk")) {
        Value num, den;
        zp2tf(mr, sys.field("z"), sys.field("p"), sys.field("k"),
              &num, &den);
        tf2ss(mr, num, den, &Av, &Bv, &Cv, &Dv);
    } else {
        throw Error("c2d/d2c: expected an LTI struct (tf/zpk/ss)",
                    0, 0, "discretize", "", "m:control:kind");
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
                    0, 0, "c2d", "", "m:c2d:singular");
    Ad = X;

    // B_d = √Ts · inv(M_minus) · B.
    Mat Mc2 = Mminus;
    Mat Bm(n, 0.0);
    for (size_t i = 0; i < n; ++i) Bm[i] = B[i];
    if (!solveInPlace(Mc2, Bm, n, 1))
        throw Error("c2d (tustin): I − A·Ts/2 is singular",
                    0, 0, "c2d", "", "m:c2d:singular");
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
                    0, 0, "c2d", "", "m:c2d:singular");
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
                    0, 0, "c2d", "", "m:c2d:singular");
    double cy = 0.0;
    for (size_t i = 0; i < n; ++i) cy += C[i] * yvec[i];
    Dd = D + (Ts / 2.0) * cy;
}

// Build an output struct of the requested kind from (A, B, C, D, Ts).
Value packResult(std::pmr::memory_resource *mr,
                 const Mat &Ad, const Vec &Bd,
                 const Vec &Cd, double Dd,
                 size_t n, double Ts,
                 const std::string &origKind)
{
    Value Av = matFromVec(mr, n, n, Ad);
    Value Bv = matFromVec(mr, n, 1, Bd);
    Value Cv = matFromVec(mr, 1, n, Cd);
    Value Dv = Value::scalar(Dd, mr);

    if (origKind == "ss") {
        return ss(mr, Av, Bv, Cv, Dv, Ts);
    }
    // Convert (A, B, C, D) → (num, den) via libs/control's ss2tf.
    Value numV, denV;
    ss2tf(mr, Av, Bv, Cv, Dv, /*iu=*/1, &numV, &denV);
    if (origKind == "tf") {
        return tf(mr, numV, denV, Ts);
    }
    if (origKind == "zpk") {
        Value zV = builtin::roots(mr, numV);
        Value pV = builtin::roots(mr, denV);
        // Gain = num(1)/den(1) (after stripping leading zeros).
        Vec numVec = coeffsReal(numV);
        Vec denVec = coeffsReal(denV);
        size_t in = 0; while (in + 1 < numVec.size() && numVec[in] == 0.0) ++in;
        size_t id = 0; while (id + 1 < denVec.size() && denVec[id] == 0.0) ++id;
        const double k = (in < numVec.size() && id < denVec.size())
                         ? numVec[in] / denVec[id] : 0.0;
        return zpk(mr, zV, pV, Value::scalar(k, mr), Ts);
    }
    return ss(mr, Av, Bv, Cv, Dv, Ts);
}

} // anonymous

Value c2d(std::pmr::memory_resource *mr,
          const Value &sys, double Ts, const std::string &method)
{
    if (Ts <= 0.0)
        throw Error("c2d: Ts must be positive",
                    0, 0, "c2d", "", "m:c2d:Ts");
    SS s = toSSiso(mr, sys);
    if (s.Ts > 0.0)
        throw Error("c2d: input system is already discrete (Ts > 0)",
                    0, 0, "c2d", "", "m:c2d:already_discrete");

    Mat Ad; Vec Bd, Cd; double Dd;
    if (method.empty() || method == "zoh") {
        zohDiscretise(s.A, s.B, s.n, Ts, Ad, Bd);
        // ZOH leaves C and D untouched.
        Cd = s.C;
        Dd = s.D;
    } else if (method == "tustin" || method == "bilinear") {
        tustinDiscretise(s.A, s.B, s.C, s.D, s.n, Ts, Ad, Bd, Cd, Dd);
    } else {
        throw Error("c2d: method must be 'zoh' or 'tustin'",
                    0, 0, "c2d", "", "m:c2d:method");
    }

    const std::string origKind = sys.isStruct() && sys.hasField("kind")
                                 ? sys.field("kind").toString()
                                 : std::string("ss");
    return packResult(mr, Ad, Bd, Cd, Dd, s.n, Ts, origKind);
}

Value d2c(std::pmr::memory_resource *mr,
          const Value &sys, const std::string &method)
{
    SS s = toSSiso(mr, sys);
    if (s.Ts <= 0.0)
        throw Error("d2c: input system is already continuous (Ts == 0)",
                    0, 0, "d2c", "", "m:d2c:already_continuous");
    if (!method.empty() && method != "tustin" && method != "bilinear")
        throw Error("d2c: only 'tustin' is supported",
                    0, 0, "d2c", "", "m:d2c:method");
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
                    0, 0, "d2c", "", "m:d2c:singular");
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
                    0, 0, "d2c", "", "m:d2c:singular");
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
                    0, 0, "d2c", "", "m:d2c:singular");
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
    return packResult(mr, Ac, Bc, Cc, Dc, s.n, 0.0, origKind);
}

namespace detail {

static std::string argString(const Value &v) {
    if (!v.isChar() && !v.isString())
        throw Error("control discretize: expected a string method",
                    0, 0, "discretize", "", "m:discretize:type");
    return v.toString();
}

void c2d_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.size() < 2)
        throw Error("c2d: requires (sys, Ts [, method])",
                    0, 0, "c2d", "", "m:c2d:nargin");
    std::string method;
    if (a.size() >= 3 && !a[2].isEmpty()) method = argString(a[2]);
    o[0] = c2d(c.engine->resource(), a[0], a[1].toScalar(), method);
}

void d2c_reg(Span<const Value> a, size_t, Span<Value> o, CallContext &c)
{
    if (a.empty())
        throw Error("d2c: requires (sys [, method])",
                    0, 0, "d2c", "", "m:d2c:nargin");
    std::string method;
    if (a.size() >= 2 && !a[1].isEmpty()) method = argString(a[1]);
    o[0] = d2c(c.engine->resource(), a[0], method);
}

} // namespace detail

} // namespace numkit::control
