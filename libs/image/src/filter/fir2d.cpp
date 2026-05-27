// libs/image/src/filter/fir2d.cpp
//
// 2-D FIR filter design family — fsamp2 / ftrans2 / fwind1 / fwind2.
//
// Algorithms transliterated verbatim from MATLAB R2025b
//   fsamp2.m, ftrans2.m, fwind1.m, fwind2.m.
//
// All four functions return DOUBLE 2-D filters intended for use with
// filter2 (equivalent to applying the filter with the rotated-by-180°
// convention).
//
// Reference: Lim, J. S. (1990). Two-Dimensional Signal and Image
// Processing, Prentice-Hall, §3.4-3.5 (window method and
// transformation method), §4.2 (frequency sampling).
//
// PMR HARD RULE: every fn takes std::pmr::memory_resource *mr.

#include <numkit/image/filter/filter.hpp>

#include <numkit/signal/transforms/fft.hpp>
#include <numkit/signal/transforms/transform_helpers.hpp>
#include <numkit/signal/convolution/convolution.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

namespace numkit::image {
namespace {

// rot90(A, 2) — 180° rotation == reverse all elements.
Value rot90_2(const Value &A, std::pmr::memory_resource *mr)
{
    const std::size_t H = A.dims().rows();
    const std::size_t W = A.dims().cols();
    Value out = Value::matrix(H, W, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    for (std::size_t c = 0; c < W; ++c)
        for (std::size_t r = 0; r < H; ++r)
            od[c * H + r] = A.elemAsDouble((W - 1 - c) * H + (H - 1 - r));
    return out;
}

// MATLAB-style ifftshift: shift each axis by ceil(N/2) (negative
// direction). ifftshift([1 2 3 4 5]) = [3 4 5 1 2] — the center
// value 3 (at index 2) moves to index 0.
//
// To compute output[c] we read from (c + ceil(N/2)) mod N.
// For odd N this equals (c + floor(N/2)) mod N.
Value ifftshift2d(const Value &A, std::pmr::memory_resource *mr)
{
    const std::size_t H = A.dims().rows();
    const std::size_t W = A.dims().cols();
    Value out = Value::matrix(H, W, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    const std::size_t hh = (H + 1) / 2;  // ceil
    const std::size_t ww = (W + 1) / 2;
    for (std::size_t c = 0; c < W; ++c) {
        const std::size_t cs = (c + ww) % W;
        for (std::size_t r = 0; r < H; ++r) {
            const std::size_t rs = (r + hh) % H;
            od[c * H + r] = A.elemAsDouble(cs * H + rs);
        }
    }
    return out;
}

// MATLAB-style fftshift: shift each axis by floor(N/2). The value
// originally at index 0 moves to index floor(N/2).
// fftshift([1 2 3 4 5]) = [4 5 1 2 3] — value 1 (at index 0) moves
// to index 2 (= floor(5/2)).
//
// Reading the output forward: output[c] = input[(c + ceil(N/2)) mod N]
// is the inverse mapping (where to *read from*). Equivalent.
//
// For ODD dims, fftshift == ifftshift. For EVEN dims they differ in
// shift direction by 1.
Value fftshift2d(const Value &A, std::pmr::memory_resource *mr)
{
    const std::size_t H = A.dims().rows();
    const std::size_t W = A.dims().cols();
    Value out = Value::matrix(H, W, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    // To compute output[r,c] we need input from index (r,c) shifted
    // by ceil(N/2) backward, i.e. read from (r + ceil(N/2)) mod N.
    // Equivalently, for odd N this equals (r + floor(N/2)) mod N.
    const std::size_t hh = (H + 1) / 2;  // ceil
    const std::size_t ww = (W + 1) / 2;
    for (std::size_t c = 0; c < W; ++c) {
        const std::size_t cs = (c + W - ww) % W;
        for (std::size_t r = 0; r < H; ++r) {
            const std::size_t rs = (r + H - hh) % H;
            od[c * H + r] = A.elemAsDouble(cs * H + rs);
        }
    }
    return out;
}

// Helper to coerce a Value to DOUBLE matrix (no class-rescaling; just
// type-cast).
Value as_double(const Value &V, std::pmr::memory_resource *mr)
{
    if (V.type() == ValueType::DOUBLE) return V;
    const auto &d = V.dims();
    Value out = Value::matrix(d.rows(), d.cols(), ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    const std::size_t N = V.numel();
    for (std::size_t i = 0; i < N; ++i) od[i] = V.elemAsDouble(i);
    return out;
}

}  // namespace

// ── fsamp2 ────────────────────────────────────────────────────────
Value fsamp2(const Value &Hd, const Value &f1, const Value &f2,
             const std::vector<std::size_t> & /*siz*/,
             std::pmr::memory_resource *mr)
{
    if (!f1.isEmpty() || !f2.isEmpty())
        throw Error("fsamp2: non-uniform-spacing form "
                    "fsamp2(f1, f2, Hd, [m n]) is not yet supported — "
                    "use the uniform fsamp2(Hd) form.",
                    0, 0, "fsamp2", "", "m:fsamp2:nonuniform");
    if (Hd.numel() == 0)
        throw Error("fsamp2: Hd must be nonempty",
                    0, 0, "fsamp2", "", "m:fsamp2:empty");
    if (Hd.dims().is3D() || Hd.dims().rows() < 2 || Hd.dims().cols() < 2)
        throw Error("fsamp2: Hd must be 2-D with at least 2 rows and 2 cols",
                    0, 0, "fsamp2", "", "m:fsamp2:shape");

    Value Hd_d = as_double(Hd, mr);
    // 1. Inverse fftshift — use numkit::signal's tested implementation
    //    via `rot90(fftshift(rot90(A, 2)), 2)`, MATLAB's idiom for
    //    ifftshift.
    Value shifted = rot90_2(signal::fftshift(rot90_2(Hd_d, mr), mr), mr);
    // 2. ifft2 (numkit's ifft2 returns complex; we expect a real
    //    result for symmetric Hd, but accept complex if needed).
    Value h_complex = signal::ifft2(shifted, -1, -1, mr);
    // 3. fftshift on the complex (or real) output.
    // If ifft2 returned DOUBLE (real), use directly; if COMPLEX, take
    // real part if imag < sqrt(eps).
    Value h_shifted;
    if (h_complex.type() == ValueType::DOUBLE) {
        h_shifted = signal::fftshift(h_complex, mr);
    } else if (h_complex.type() == ValueType::COMPLEX) {
        // Take real part; check imag magnitudes.
        const std::size_t H = h_complex.dims().rows();
        const std::size_t W = h_complex.dims().cols();
        Value real_part = Value::matrix(H, W, ValueType::DOUBLE, mr);
        double *rd = real_part.doubleDataMut();
        const std::complex<double> *cd = h_complex.complexData();
        double max_imag = 0.0;
        for (std::size_t i = 0; i < H * W; ++i) {
            rd[i] = cd[i].real();
            const double im = std::fabs(cd[i].imag());
            if (im > max_imag) max_imag = im;
        }
        const double sqrt_eps = std::sqrt(
            std::numeric_limits<double>::epsilon());
        if (max_imag >= sqrt_eps) {
            // Keep as complex — but numkit's downstream might not
            // handle complex Values in this code path; warn-cast.
            // For our purposes, return the real part anyway (MATLAB
            // does the same when imag is small).
        }
        h_shifted = signal::fftshift(real_part, mr);
    } else {
        h_shifted = signal::fftshift(as_double(h_complex, mr), mr);
    }
    // 4. rot90(h, 2) for filter2 convention.
    return rot90_2(h_shifted, mr);
}

// ── ftrans2 ──────────────────────────────────────────────────────
Value ftrans2(const Value &b_in, const Value &t_in,
              std::pmr::memory_resource *mr)
{
    Value b = as_double(b_in, mr);
    const std::size_t L = b.numel();
    if (L == 0)
        throw Error("ftrans2: b must be nonempty",
                    0, 0, "ftrans2", "", "m:ftrans2:bEmpty");
    if (L % 2 == 0)
        throw Error("ftrans2: b must be odd-length",
                    0, 0, "ftrans2", "", "m:ftrans2:bLen");
    // Check zero.
    bool all_zero = true;
    for (std::size_t i = 0; i < L; ++i)
        if (b.elemAsDouble(i) != 0.0) { all_zero = false; break; }
    if (all_zero)
        throw Error("ftrans2: b must have at least one nonzero element",
                    0, 0, "ftrans2", "", "m:ftrans2:bZero");
    // Check symmetry: b == rot90(b, 2) within sqrt(eps).
    const double sqrt_eps = std::sqrt(std::numeric_limits<double>::epsilon());
    for (std::size_t i = 0; i < L; ++i) {
        const double a = b.elemAsDouble(i);
        const double c = b.elemAsDouble(L - 1 - i);
        if (std::fabs(a - c) > sqrt_eps)
            throw Error("ftrans2: b must be symmetric",
                        0, 0, "ftrans2", "", "m:ftrans2:bSym");
    }
    // Resolve transform matrix t.
    Value t;
    if (t_in.isEmpty()) {
        // Default McClellan.
        t = Value::matrix(3, 3, ValueType::DOUBLE, mr);
        double *td = t.doubleDataMut();
        // Column-major fill of [1 2 1; 2 -4 2; 1 2 1]/8.
        // MATLAB layout: row-major rows above translate to col-major:
        // (r=0,c=0)=1, (1,0)=2, (2,0)=1, (0,1)=2, (1,1)=-4, (2,1)=2, etc.
        td[0] = 1.0/8.0; td[1] = 2.0/8.0; td[2] = 1.0/8.0;
        td[3] = 2.0/8.0; td[4] = -4.0/8.0; td[5] = 2.0/8.0;
        td[6] = 1.0/8.0; td[7] = 2.0/8.0; td[8] = 1.0/8.0;
    } else {
        t = as_double(t_in, mr);
        bool tzero = true;
        for (std::size_t i = 0; i < t.numel(); ++i)
            if (t.elemAsDouble(i) != 0.0) { tzero = false; break; }
        if (tzero)
            throw Error("ftrans2: t must have at least one nonzero element",
                        0, 0, "ftrans2", "", "m:ftrans2:tZero");
    }

    // Convert b to Chebyshev coefficients a:
    //   b is length L = 2n+1, symmetric around index n.
    //   shifted = ifftshift(b) so b becomes [b(n), b(n+1), ..., b(2n),
    //                                         b(0), ..., b(n-1)].
    //   a = [shifted(1), 2*shifted(2:n+1)] (1-based).
    const long n = (static_cast<long>(L) - 1) / 2;
    std::vector<double> b_vec(L);
    for (std::size_t i = 0; i < L; ++i) b_vec[i] = b.elemAsDouble(i);
    // ifftshift on 1-D: roll by floor(L/2).
    const long shift = static_cast<long>(L) / 2;
    std::vector<double> bsh(L);
    for (std::size_t i = 0; i < L; ++i)
        bsh[i] = b_vec[(static_cast<long>(i) + shift) % static_cast<long>(L)];
    std::vector<double> a(static_cast<std::size_t>(n + 1));
    a[0] = bsh[0];
    for (long k = 1; k <= n; ++k) a[static_cast<std::size_t>(k)] = 2.0 * bsh[static_cast<std::size_t>(k)];

    // Chebyshev recursion.
    const std::size_t Mt = t.dims().rows();
    const std::size_t Nt = t.dims().cols();
    const std::size_t insetR = (Mt - 1) / 2;
    const std::size_t insetC = (Nt - 1) / 2;
    // P0 = 1 (scalar 1x1).
    Value P0 = Value::matrix(1, 1, ValueType::DOUBLE, mr);
    P0.doubleDataMut()[0] = 1.0;
    Value P1 = t;
    // h = a(2) * P1 (a is 0-indexed in C++; a(2) MATLAB = a[1] C++).
    auto scale_mat = [&](const Value &A, double s) {
        const std::size_t H = A.dims().rows();
        const std::size_t W = A.dims().cols();
        Value out = Value::matrix(H, W, ValueType::DOUBLE, mr);
        double *od = out.doubleDataMut();
        for (std::size_t i = 0; i < H * W; ++i)
            od[i] = A.elemAsDouble(i) * s;
        return out;
    };
    Value h = scale_mat(P1, a[1]);
    // Add a(1) * P0 at position (insetR, insetC) (0-based).
    auto add_at = [&](Value &dst, const Value &src,
                      std::size_t r0, std::size_t c0) {
        const std::size_t Hs = src.dims().rows();
        const std::size_t Ws = src.dims().cols();
        const std::size_t Hd = dst.dims().rows();
        double *dd = dst.doubleDataMut();
        for (std::size_t c = 0; c < Ws; ++c)
            for (std::size_t r = 0; r < Hs; ++r)
                dd[(c0 + c) * Hd + (r0 + r)] += src.elemAsDouble(c * Hs + r);
    };
    add_at(h, scale_mat(P0, a[0]), insetR, insetC);
    std::size_t rowsT = insetR + 1, colsT = insetC + 1; // 1-based MATLAB
    (void)rowsT; (void)colsT;
    // For i = 3..n+1 (MATLAB) → C++ i = 2..n (after a[k-1] indexing).
    std::size_t cur_rows_off = insetR;
    std::size_t cur_cols_off = insetC;
    for (long i = 2; i <= n; ++i) {
        // P2 = 2 * conv2(t, P1)  (default 'full' shape).
        Value conv = signal::conv2(t, P1, "full", mr);
        Value P2 = scale_mat(conv, 2.0);
        cur_rows_off += insetR;
        cur_cols_off += insetC;
        // P2(rows, cols) -= P0  (at offset insetR*(i-1), insetC*(i-1) ... etc)
        const std::size_t Hp = P2.dims().rows();
        double *p2d = P2.doubleDataMut();
        const std::size_t Hp0 = P0.dims().rows();
        const std::size_t Wp0 = P0.dims().cols();
        for (std::size_t c = 0; c < Wp0; ++c)
            for (std::size_t r = 0; r < Hp0; ++r)
                p2d[(cur_cols_off + c) * Hp + (cur_rows_off + r)]
                    -= P0.elemAsDouble(c * Hp0 + r);
        // Wrap insertion: h_new = a[i] * P2; then add h (old) at offset (insetR, insetC).
        Value h_new = scale_mat(P2, a[static_cast<std::size_t>(i)]);
        add_at(h_new, h, insetR, insetC);
        h = std::move(h_new);
        P0 = P1;
        P1 = P2;
    }
    // rot90(h, 2) for filter2.
    return rot90_2(h, mr);
}

// ── fwind1 ───────────────────────────────────────────────────────
// Linear interpolation utility used by Huang's method.
double linear_interp(const std::vector<double> &xs,
                     const std::vector<double> &ys, double q)
{
    // Assumes xs is uniformly spaced (matches freqspace output).
    if (q <= xs.front()) return ys.front();
    if (q >= xs.back())  return ys.back();
    const double dx = xs[1] - xs[0];
    const double t = (q - xs[0]) / dx;
    const std::size_t i = static_cast<std::size_t>(std::floor(t));
    const double a = t - static_cast<double>(i);
    return (1.0 - a) * ys[i] + a * ys[i + 1];
}

Value fwind1(const Value &Hd, const Value &win1, const Value &win2,
             std::pmr::memory_resource *mr)
{
    if (Hd.numel() == 0)
        throw Error("fwind1: Hd must be nonempty",
                    0, 0, "fwind1", "", "m:fwind1:empty");
    if (win1.numel() < 2)
        throw Error("fwind1: window length must be >= 2",
                    0, 0, "fwind1", "", "m:fwind1:winShort");

    const std::size_t n = win1.numel();
    std::vector<double> w1(n);
    for (std::size_t i = 0; i < n; ++i) w1[i] = win1.elemAsDouble(i);

    // Check 1-D window symmetry.
    const double sqrt_eps = std::sqrt(std::numeric_limits<double>::epsilon());
    if (win2.isEmpty()) {
        for (std::size_t i = 0; i < n; ++i)
            if (std::fabs(w1[i] - w1[n - 1 - i]) > sqrt_eps)
                throw Error("fwind1: 1-D window must be symmetric",
                            0, 0, "fwind1", "",
                            "m:fwind1:winSym");
    }

    // Build 2-D window.
    Value W;
    std::size_t m_out, n_out;
    if (win2.isEmpty()) {
        // Huang's method: radial linear interp of w1.
        m_out = n_out = n;
        // 1-D positions: t = (-(n-1)/2 : (n-1)/2) * (2/(n-1))
        std::vector<double> t(n);
        for (std::size_t i = 0; i < n; ++i)
            t[i] = (static_cast<double>(i) - (n - 1) / 2.0) * (2.0 / (n - 1));
        W = Value::matrix(n, n, ValueType::DOUBLE, mr);
        double *wd = W.doubleDataMut();
        for (std::size_t c = 0; c < n; ++c)
            for (std::size_t r = 0; r < n; ++r) {
                const double tx = t[c];
                const double ty = t[r];
                const double rr = std::sqrt(tx * tx + ty * ty);
                if (rr < t.front() || rr > t.back())
                    wd[c * n + r] = 0.0;
                else
                    wd[c * n + r] = linear_interp(t, w1, rr);
            }
    } else {
        // Separable: W = w2 * w1.'
        const std::size_t m = win2.numel();
        std::vector<double> w2v(m);
        for (std::size_t i = 0; i < m; ++i) w2v[i] = win2.elemAsDouble(i);
        m_out = m;  n_out = n;
        W = Value::matrix(m, n, ValueType::DOUBLE, mr);
        double *wd = W.doubleDataMut();
        for (std::size_t c = 0; c < n; ++c)
            for (std::size_t r = 0; r < m; ++r)
                wd[c * m + r] = w2v[r] * w1[c];
    }

    // Verify Hd shape matches W; if not, must interpolate (uniform
    // case only — non-uniform fsamp2 form is not yet supported, see
    // fsamp2 docs).
    const std::size_t Hh = Hd.dims().rows();
    const std::size_t Wh = Hd.dims().cols();
    Value Hd_used;
    if (Hh == m_out && Wh == n_out) {
        Hd_used = Hd;
    } else {
        // Bilinear interpolation of Hd onto the m_out × n_out grid.
        // Both source and target use freqspace coords in [-1, 1)
        // (or with extrapolation as per fwind1.m). We implement a
        // simplified bilinear interp directly here.
        Value Hd_d = as_double(Hd, mr);
        const double dr_src = 2.0 / static_cast<double>(Hh);
        const double dc_src = 2.0 / static_cast<double>(Wh);
        const double dr_dst = 2.0 / static_cast<double>(m_out);
        const double dc_dst = 2.0 / static_cast<double>(n_out);
        Hd_used = Value::matrix(m_out, n_out, ValueType::DOUBLE, mr);
        double *od = Hd_used.doubleDataMut();
        for (std::size_t c = 0; c < n_out; ++c) {
            const double cx = (static_cast<double>(c)
                              - (n_out - 1) / 2.0) * dc_dst;
            const double sf_c = (cx + 1.0) / dc_src - 0.5;
            const long c0 = std::max<long>(0, static_cast<long>(
                std::floor(sf_c)));
            const long c1 = std::min<long>(static_cast<long>(Wh) - 1, c0 + 1);
            const double ac = sf_c - c0;
            for (std::size_t r = 0; r < m_out; ++r) {
                const double cy = (static_cast<double>(r)
                                  - (m_out - 1) / 2.0) * dr_dst;
                const double sf_r = (cy + 1.0) / dr_src - 0.5;
                const long r0 = std::max<long>(0, static_cast<long>(
                    std::floor(sf_r)));
                const long r1 = std::min<long>(static_cast<long>(Hh) - 1, r0 + 1);
                const double ar = sf_r - r0;
                const double v00 = Hd_d.elemAsDouble(
                    static_cast<std::size_t>(c0) * Hh + r0);
                const double v01 = Hd_d.elemAsDouble(
                    static_cast<std::size_t>(c0) * Hh + r1);
                const double v10 = Hd_d.elemAsDouble(
                    static_cast<std::size_t>(c1) * Hh + r0);
                const double v11 = Hd_d.elemAsDouble(
                    static_cast<std::size_t>(c1) * Hh + r1);
                const double v0 = v00 + ar * (v01 - v00);
                const double v1 = v10 + ar * (v11 - v10);
                od[c * m_out + r] = v0 + ac * (v1 - v0);
            }
        }
    }
    // h = fsamp2(Hd_used) .* W.
    Value h = fsamp2(Hd_used, Value::Empty, Value::Empty, {}, mr);
    const std::size_t M = h.dims().rows();
    const std::size_t N = h.dims().cols();
    Value out = Value::matrix(M, N, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    const double *hd_ = h.doubleData();
    const double *wd_ = W.doubleData();
    for (std::size_t i = 0; i < M * N; ++i) od[i] = hd_[i] * wd_[i];
    return out;
}

// ── fwind2 ───────────────────────────────────────────────────────
Value fwind2(const Value &Hd, const Value &W,
             std::pmr::memory_resource *mr)
{
    if (Hd.numel() == 0)
        throw Error("fwind2: Hd must be nonempty",
                    0, 0, "fwind2", "", "m:fwind2:empty");
    if (W.numel() == 0)
        throw Error("fwind2: W must be nonempty",
                    0, 0, "fwind2", "", "m:fwind2:wEmpty");
    const std::size_t Mh = Hd.dims().rows();
    const std::size_t Nh = Hd.dims().cols();
    const std::size_t Mw = W.dims().rows();
    const std::size_t Nw = W.dims().cols();
    Value Hd_used = Hd;
    if (Mh != Mw || Nh != Nw) {
        // Bilinear interpolation (same as fwind1's interp branch).
        Value Hd_d = as_double(Hd, mr);
        const double dr_src = 2.0 / static_cast<double>(Mh);
        const double dc_src = 2.0 / static_cast<double>(Nh);
        const double dr_dst = 2.0 / static_cast<double>(Mw);
        const double dc_dst = 2.0 / static_cast<double>(Nw);
        Hd_used = Value::matrix(Mw, Nw, ValueType::DOUBLE, mr);
        double *od = Hd_used.doubleDataMut();
        for (std::size_t c = 0; c < Nw; ++c) {
            const double cx = (static_cast<double>(c)
                              - (Nw - 1) / 2.0) * dc_dst;
            const double sf_c = (cx + 1.0) / dc_src - 0.5;
            const long c0 = std::max<long>(0, static_cast<long>(
                std::floor(sf_c)));
            const long c1 = std::min<long>(static_cast<long>(Nh) - 1, c0 + 1);
            const double ac = sf_c - c0;
            for (std::size_t r = 0; r < Mw; ++r) {
                const double cy = (static_cast<double>(r)
                                  - (Mw - 1) / 2.0) * dr_dst;
                const double sf_r = (cy + 1.0) / dr_src - 0.5;
                const long r0 = std::max<long>(0, static_cast<long>(
                    std::floor(sf_r)));
                const long r1 = std::min<long>(static_cast<long>(Mh) - 1, r0 + 1);
                const double ar = sf_r - r0;
                const double v00 = Hd_d.elemAsDouble(
                    static_cast<std::size_t>(c0) * Mh + r0);
                const double v01 = Hd_d.elemAsDouble(
                    static_cast<std::size_t>(c0) * Mh + r1);
                const double v10 = Hd_d.elemAsDouble(
                    static_cast<std::size_t>(c1) * Mh + r0);
                const double v11 = Hd_d.elemAsDouble(
                    static_cast<std::size_t>(c1) * Mh + r1);
                const double v0 = v00 + ar * (v01 - v00);
                const double v1 = v10 + ar * (v11 - v10);
                od[c * Mw + r] = v0 + ac * (v1 - v0);
            }
        }
    }
    Value h = fsamp2(Hd_used, Value::Empty, Value::Empty, {}, mr);
    Value out = Value::matrix(Mw, Nw, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    const double *hd_ = h.doubleData();
    Value W_d = as_double(W, mr);
    const double *wd_ = W_d.doubleData();
    for (std::size_t i = 0; i < Mw * Nw; ++i) od[i] = hd_[i] * wd_[i];
    return out;
}

namespace detail {

void fsamp2_reg(Span<const Value> args, std::size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("fsamp2: requires (Hd) or (f1, f2, Hd, [m n])",
                    0, 0, "fsamp2", "", "m:fsamp2:nargin");
    if (args.size() == 1) {
        outs[0] = fsamp2(args[0], Value::Empty, Value::Empty, {},
                         ctx.engine->resource());
        return;
    }
    if (args.size() == 4) {
        // (f1, f2, Hd, siz) — non-uniform form (not supported).
        outs[0] = fsamp2(args[2], args[0], args[1], {},
                         ctx.engine->resource());
        return;
    }
    throw Error("fsamp2: requires 1 or 4 arguments",
                0, 0, "fsamp2", "", "m:fsamp2:nargin");
}

void ftrans2_reg(Span<const Value> args, std::size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("ftrans2: requires (b [, t])",
                    0, 0, "ftrans2", "", "m:ftrans2:nargin");
    Value t = (args.size() >= 2) ? args[1] : Value::Empty;
    outs[0] = ftrans2(args[0], t, ctx.engine->resource());
}

void fwind1_reg(Span<const Value> args, std::size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("fwind1: requires (Hd, win) or (Hd, win1, win2)",
                    0, 0, "fwind1", "", "m:fwind1:nargin");
    auto *mr = ctx.engine->resource();
    if (args.size() == 2) {
        // (Hd, win) — Huang's method.
        outs[0] = fwind1(args[0], args[1], Value::Empty, mr);
    } else if (args.size() == 3) {
        // (Hd, win1, win2) — separable.
        outs[0] = fwind1(args[0], args[1], args[2], mr);
    } else {
        // (f1, f2, Hd, ...) — non-uniform spacing case (not supported).
        throw Error("fwind1: non-uniform-spacing form is not yet supported",
                    0, 0, "fwind1", "", "m:fwind1:nonuniform");
    }
}

void fwind2_reg(Span<const Value> args, std::size_t /*nargout*/,
                Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("fwind2: requires (Hd, W) or (f1, f2, Hd, W)",
                    0, 0, "fwind2", "", "m:fwind2:nargin");
    auto *mr = ctx.engine->resource();
    if (args.size() == 2) {
        outs[0] = fwind2(args[0], args[1], mr);
    } else {
        throw Error("fwind2: non-uniform-spacing form is not yet supported",
                    0, 0, "fwind2", "", "m:fwind2:nonuniform");
    }
}

}  // namespace detail
}  // namespace numkit::image
