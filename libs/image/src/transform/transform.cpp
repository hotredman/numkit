// libs/image/src/transform/transform.cpp
//
// 2-D DCT/IDCT and the dctmtx generator. Internally these compose the
// 1-D transforms from libs/signal — DCT-II is separable, so a 2-D
// transform is just two passes of 1-D DCT (rows then columns, or vice
// versa). Storage is column-major throughout, matching the rest of
// numkit's Value layout.

#include <numkit/image/transform/transform.hpp>
#include <numkit/image/filter/filter.hpp>

#include <numkit/builtin/language/arrays/manip.hpp>
#include <numkit/signal/transforms/dct.hpp>
#include <numkit/signal/transforms/fft.hpp>
#include <numkit/signal/convolution/convolution.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::image {

namespace {

// Apply 1-D DCT (or IDCT) to every column of A and write results to
// `out`. Columns of `A` are length-M contiguous slices in column-major
// storage. We slice each column into a temporary M×1 vector, run
// signal::dct/idct on it, and copy the result back. Allocations come
// from the engine arena (mr) so per-call cost stays bounded.
template <typename Fn1D>
Value apply_along_columns(const Value &A, Fn1D &&fn1d, std::pmr::memory_resource *mr)
{
    const size_t M = A.dims().rows();
    const size_t N = A.dims().cols();
    Value out = Value::matrix(M, N, ValueType::DOUBLE, mr);
    if (M == 0 || N == 0) return out;
    double *dst = out.doubleDataMut();

    auto col = Value::matrix(M, 1, ValueType::DOUBLE, mr);
    double *cd = col.doubleDataMut();

    for (size_t c = 0; c < N; ++c) {
        for (size_t r = 0; r < M; ++r)
            cd[r] = A.elemAsDouble(c * M + r);
        Value Y = fn1d(col, mr);
        const double *yd = Y.doubleData();
        for (size_t r = 0; r < M; ++r)
            dst[c * M + r] = yd[r];
    }
    return out;
}

// Apply 1-D DCT/IDCT to every row of A. Allocates a length-N row
// buffer per row.
template <typename Fn1D>
Value apply_along_rows(const Value &A, Fn1D &&fn1d, std::pmr::memory_resource *mr)
{
    const size_t M = A.dims().rows();
    const size_t N = A.dims().cols();
    Value out = Value::matrix(M, N, ValueType::DOUBLE, mr);
    if (M == 0 || N == 0) return out;
    double *dst = out.doubleDataMut();

    auto row = Value::matrix(N, 1, ValueType::DOUBLE, mr);
    double *rd = row.doubleDataMut();

    for (size_t r = 0; r < M; ++r) {
        for (size_t c = 0; c < N; ++c)
            rd[c] = A.elemAsDouble(c * M + r);
        Value Y = fn1d(row, mr);
        const double *yd = Y.doubleData();
        for (size_t c = 0; c < N; ++c)
            dst[c * M + r] = yd[c];
    }
    return out;
}

} // anonymous

Value dct2(const Value &A, std::pmr::memory_resource *mr)
{
    // Two passes of orthonormal Type-II DCT (separable). Columns first,
    // then rows — output is identical either way.
    // Disambiguate the 1-D dct overload (a 4-arg matrix form also exists).
    using Dct1D = Value (*)(const Value &, std::pmr::memory_resource *);
    Dct1D dct1 = &numkit::signal::dct;
    Value Y = apply_along_columns(A, dct1, mr);
    return apply_along_rows(Y, dct1, mr);
}

Value idct2(const Value &A, std::pmr::memory_resource *mr)
{
    using Idct1D = Value (*)(const Value &, std::pmr::memory_resource *);
    Idct1D idct1 = &numkit::signal::idct;
    Value Y = apply_along_columns(A, idct1, mr);
    return apply_along_rows(Y, idct1, mr);
}

Value dctmtx(double Nd, std::pmr::memory_resource *mr)
{
    // MATLAB's dctmtx(N) returns an N×N matrix D whose rows are the
    // DCT-II basis vectors:
    //   D[k, n] = w[k] · cos(π · (2n+1) · k / (2N))
    //   w[0]    = sqrt(1/N),  w[k>0] = sqrt(2/N)
    // so D*x is the DCT-II of x.
    if (!(Nd > 0.0) || std::floor(Nd) != Nd)
        throw Error("dctmtx: N must be a positive integer",
                    0, 0, "dctmtx", "", "m:dctmtx:arg");
    const size_t N = static_cast<size_t>(Nd);
    Value D = Value::matrix(N, N, ValueType::DOUBLE, mr);
    if (N == 0) return D;
    double *d = D.doubleDataMut();
    const double w0 = std::sqrt(1.0 / static_cast<double>(N));
    const double wk = std::sqrt(2.0 / static_cast<double>(N));
    const double piOver2N = M_PI / (2.0 * static_cast<double>(N));
    for (size_t n = 0; n < N; ++n) {
        for (size_t k = 0; k < N; ++k) {
            const double phase = piOver2N * static_cast<double>(k)
                                 * static_cast<double>(2 * n + 1);
            const double w = (k == 0) ? w0 : wk;
            // Column-major: element (k, n) at offset n*N + k.
            d[n * N + k] = w * std::cos(phase);
        }
    }
    return D;
}

Value integralImage(const Value &I, std::pmr::memory_resource *mr)
{
    const auto &d = I.dims();
    const size_t H = d.rows();
    const size_t W = d.cols();
    const size_t H1 = H + 1;
    Value out = Value::matrix(H1, W + 1, ValueType::DOUBLE, mr);
    if (H == 0 || W == 0) return out;
    double *od = out.doubleDataMut();
    // out[r+1, c+1] = out[r, c+1] + out[r+1, c] - out[r, c] + I[r, c].
    for (size_t c = 0; c < W; ++c) {
        const size_t cb = (c + 1) * H1;
        const size_t cl = c * H1;
        for (size_t r = 0; r < H; ++r) {
            const size_t r1 = r + 1;
            od[cb + r1] = od[cl + r1] + od[cb + r] - od[cl + r]
                        + I.elemAsDouble(c * H + r);
        }
    }
    return out;
}

Value integralImage3(const Value &V, std::pmr::memory_resource *mr)
{
    const auto &d = V.dims();
    const size_t H = d.rows();
    const size_t W = d.cols();
    const size_t P = d.is3D() ? d.pages() : 1;
    const size_t H1 = H + 1;
    const size_t W1 = W + 1;
    const size_t plane1 = H1 * W1;
    const size_t planeIn = H * W;
    Value out = Value::matrix3d(H1, W1, P + 1, ValueType::DOUBLE, mr);
    if (H == 0 || W == 0 || P == 0) return out;
    double *od = out.doubleDataMut();
    auto idx = [&](size_t r, size_t c, size_t p) {
        return p * plane1 + c * H1 + r;
    };
    for (size_t p = 0; p < P; ++p) {
        const size_t p1 = p + 1;
        for (size_t c = 0; c < W; ++c) {
            const size_t c1 = c + 1;
            for (size_t r = 0; r < H; ++r) {
                const size_t r1 = r + 1;
                const double v = V.elemAsDouble(p * planeIn + c * H + r);
                od[idx(r1, c1, p1)] =
                      od[idx(r,  c1, p1)]
                    + od[idx(r1, c,  p1)]
                    + od[idx(r1, c1, p )]
                    - od[idx(r,  c,  p1)]
                    - od[idx(r,  c1, p )]
                    - od[idx(r1, c,  p )]
                    + od[idx(r,  c,  p )]
                    + v;
            }
        }
    }
    return out;
}

namespace {

// Modified Shepp-Logan ellipse parameters (Toft 1996, Table B.3).
// Each row: [I, a, b, x0, y0, phi_deg].
constexpr double kModSheppLogan[10][6] = {
    { 1.0,  0.69,   0.92,   0.0,    0.0,      0.0},
    {-0.8,  0.6624, 0.874,  0.0,   -0.0184,   0.0},
    {-0.2,  0.11,   0.31,   0.22,   0.0,    -18.0},
    {-0.2,  0.16,   0.41,  -0.22,   0.0,     18.0},
    { 0.1,  0.21,   0.25,   0.0,    0.35,     0.0},
    { 0.1,  0.046,  0.046,  0.0,    0.1,      0.0},
    { 0.1,  0.046,  0.046,  0.0,   -0.1,      0.0},
    { 0.1,  0.046,  0.023, -0.08,  -0.605,    0.0},
    { 0.1,  0.023,  0.023,  0.0,   -0.606,    0.0},
    { 0.1,  0.023,  0.046,  0.06,  -0.605,    0.0},
};

// Original Shepp-Logan (1974), with the first ellipse intensity reduced
// from 2.0 to 1.0 so the head intensity stays in [0, 1].
constexpr double kSheppLogan[10][6] = {
    { 1.0,   0.69,   0.92,   0.0,    0.0,       0.0},
    {-0.98,  0.6624, 0.874,  0.0,   -0.0184,    0.0},
    {-0.02,  0.11,   0.31,   0.22,   0.0,     -18.0},
    {-0.02,  0.16,   0.41,  -0.22,   0.0,      18.0},
    { 0.01,  0.21,   0.25,   0.0,    0.35,      0.0},
    { 0.01,  0.046,  0.046,  0.0,    0.1,       0.0},
    { 0.01,  0.046,  0.046,  0.0,   -0.1,       0.0},
    { 0.01,  0.046,  0.023, -0.08,  -0.605,     0.0},
    { 0.01,  0.023,  0.023,  0.0,   -0.606,     0.0},
    { 0.01,  0.023,  0.046,  0.06,  -0.605,     0.0},
};

Value make_ellipse_matrix(const double (*src)[6], size_t rows, std::pmr::memory_resource *mr)
{
    Value E = Value::matrix(rows, 6, ValueType::DOUBLE, mr);
    double *ed = E.doubleDataMut();
    // Column-major: ed[c * rows + r] = src[r][c].
    for (size_t r = 0; r < rows; ++r)
        for (size_t c = 0; c < 6; ++c)
            ed[c * rows + r] = src[r][c];
    return E;
}

} // anonymous

std::tuple<Value, Value>
phantom(const Value &model_or_E, size_t n, std::pmr::memory_resource *mr)
{
    if (n == 0) n = 256;

    // Determine ellipses matrix.
    Value E;
    if (model_or_E.numel() == 0) {
        E = make_ellipse_matrix(kModSheppLogan, 10, mr);
    } else if (model_or_E.isChar() || model_or_E.isString()) {
        const std::string m = model_or_E.toString();
        std::string lo;
        lo.reserve(m.size());
        for (char c : m) lo.push_back(static_cast<char>(std::tolower(c)));
        if (lo == "shepp-logan")
            E = make_ellipse_matrix(kSheppLogan, 10, mr);
        else if (lo == "modified shepp-logan")
            E = make_ellipse_matrix(kModSheppLogan, 10, mr);
        else
            throw Error("phantom: unknown MODEL", 0, 0, "phantom", "",
                        "m:phantom:model");
    } else {
        if (model_or_E.dims().cols() != 6)
            throw Error("phantom: E must be N-by-6",
                        0, 0, "phantom", "", "m:phantom:E");
        E = model_or_E;
    }

    Value head = Value::matrix(n, n, ValueType::DOUBLE, mr);
    if (n == 0) return {std::move(head), std::move(E)};
    double *hd = head.doubleDataMut();

    // Build x grid: xvals[i] = -1 + 2i/(n-1) for n > 1; -1 for n == 1.
    std::vector<double> xvals(n);
    if (n == 1) xvals[0] = -1.0;
    else
        for (size_t i = 0; i < n; ++i)
            xvals[i] = -1.0 + 2.0 * static_cast<double>(i)
                              / static_cast<double>(n - 1);

    const size_t nE = E.dims().rows();
    const double *ed = E.doubleData();
    auto Eat = [&](size_t r, size_t c) {
        return ed[c * nE + r];
    };

    for (size_t k = 0; k < nE; ++k) {
        const double I  = Eat(k, 0);
        const double a  = Eat(k, 1);
        const double b  = Eat(k, 2);
        const double x0 = Eat(k, 3);
        const double y0 = Eat(k, 4);
        const double phi = Eat(k, 5) * M_PI / 180.0;
        const double a2  = a * a;
        const double b2  = b * b;
        const double cos_p = std::cos(phi);
        const double sin_p = std::sin(phi);

        // xgrid[r, c] = xvals[c]; y = rot90(xgrid) → y[r, c] = xvals[n-1-r].
        for (size_t c = 0; c < n; ++c) {
            const double xg_c = xvals[c];
            const double x = xg_c - x0;
            const size_t col_off = c * n;
            for (size_t r = 0; r < n; ++r) {
                const double y = xvals[n - 1 - r] - y0;
                const double u = x * cos_p + y * sin_p;
                const double v = y * cos_p - x * sin_p;
                if ((u * u) / a2 + (v * v) / b2 <= 1.0)
                    hd[col_off + r] += I;
            }
        }
    }
    return {std::move(head), std::move(E)};
}

Value bestblk(const Value &IMS, double k, std::pmr::memory_resource *mr)
{
    if (IMS.numel() < 2)
        throw Error("bestblk: IMS must have at least 2 elements",
                    0, 0, "bestblk", "", "m:bestblk:size");
    if (!(k >= 1.0))
        throw Error("bestblk: K must be a positive scalar",
                    0, 0, "bestblk", "", "m:bestblk:k");

    const size_t nd = IMS.numel();
    const long long K = static_cast<long long>(std::floor(k));
    Value out = Value::matrix(1, nd, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();

    for (size_t d = 0; d < nd; ++d) {
        const long long dim = static_cast<long long>(
            std::floor(IMS.elemAsDouble(d)));
        if (dim < 1)
            throw Error("bestblk: IMS entries must be positive",
                        0, 0, "bestblk", "", "m:bestblk:vals");
        if (dim <= K) { od[d] = static_cast<double>(dim); continue; }

        // Scan p = K, K-1, ..., ceil(min(dim/10, K/2)). Pick largest p
        // with smallest mod(-dim, p); strict-less on the descending
        // walk keeps the higher p on ties. Octave's : range stops at
        // ceil(lo_double) when the lower bound is non-integer.
        const double lo_d = std::min(static_cast<double>(dim) / 10.0,
                                     static_cast<double>(K) / 2.0);
        long long lo = static_cast<long long>(std::ceil(lo_d));
        if (lo < 1) lo = 1;
        long long bestP   = K;
        long long bestPad = ((K - dim % K) % K + K) % K;
        for (long long p = K - 1; p >= lo; --p) {
            const long long pad = ((p - dim % p) % p + p) % p;
            if (pad < bestPad) {
                bestPad = pad;
                bestP   = p;
            }
        }
        od[d] = static_cast<double>(bestP);
    }
    return out;
}

Value fftconv2(const Value &A, const Value &B, const std::string &shape, std::pmr::memory_resource *mr)
{
    const size_t ra = A.dims().rows(), ca = A.dims().cols();
    const size_t rb = B.dims().rows(), cb = B.dims().cols();
    if (ra == 0 || ca == 0 || rb == 0 || cb == 0)
        return Value::matrix(0, 0, ValueType::COMPLEX, mr);

    const size_t Hf = ra + rb - 1;
    const size_t Wf = ca + cb - 1;
    // signal::fft only handles power-of-2 lengths reliably right now,
    // so round both axes up to the next power of 2 for the transform
    // and crop afterwards. Linear-convolution semantics are preserved
    // because the input is zero-padded.
    auto next_pow2 = [](size_t n) {
        size_t p = 1;
        while (p < n) p <<= 1;
        return p ? p : 1;
    };
    const size_t Hp = next_pow2(Hf);
    const size_t Wp = next_pow2(Wf);

    auto pad_post = [&](const Value &X, size_t Hpad, size_t Wpad) {
        return padarray(X, {(int)(Hpad - X.dims().rows()), (int)(Wpad - X.dims().cols())}, PadMode::Constant, 0.0, "post", mr);
    };
    Value Ap = pad_post(A, Hp, Wp);
    Value Bp = pad_post(B, Hp, Wp);

    Value FA = signal::fft2(Ap, -1, -1, mr);
    Value FB = signal::fft2(Bp, -1, -1, mr);

    Value FY = Value::matrix(Hp, Wp, ValueType::COMPLEX, mr);
    {
        const Complex *a_ = FA.complexData();
        const Complex *b_ = FB.complexData();
        Complex *y_       = FY.complexDataMut();
        const size_t N = Hp * Wp;
        for (size_t i = 0; i < N; ++i) y_[i] = a_[i] * b_[i];
    }

    Value Yfull = signal::ifft2(FY, -1, -1, mr);

    // Crop back to the linear-convolution size Hf × Wf.
    Value Y = Value::matrix(Hf, Wf, Yfull.type(), mr);
    if (Yfull.type() == ValueType::COMPLEX) {
        const Complex *src = Yfull.complexData();
        Complex *dst       = Y.complexDataMut();
        for (size_t c = 0; c < Wf; ++c)
            for (size_t r = 0; r < Hf; ++r)
                dst[c * Hf + r] = src[c * Hp + r];
    } else {
        const double *src = Yfull.doubleData();
        double *dst       = Y.doubleDataMut();
        for (size_t c = 0; c < Wf; ++c)
            for (size_t r = 0; r < Hf; ++r)
                dst[c * Hf + r] = src[c * Hp + r];
    }

    std::string sh;
    sh.reserve(shape.size());
    for (char c : shape) sh.push_back(static_cast<char>(std::tolower(c)));
    if (sh.empty()) sh = "full";
    if (sh == "full") return Y;

    size_t r0 = 0, c0 = 0, outH = 0, outW = 0;
    if (sh == "same") {
        r0 = rb / 2;
        c0 = cb / 2;
        outH = ra;
        outW = ca;
    } else if (sh == "valid") {
        if (ra < rb || ca < cb)
            return Value::matrix(0, 0, Y.type(), mr);
        r0 = rb - 1;
        c0 = cb - 1;
        outH = ra - rb + 1;
        outW = ca - cb + 1;
    } else {
        throw Error("fftconv2: shape must be 'full', 'same', or 'valid'",
                    0, 0, "fftconv2", "", "m:fftconv2:shape");
    }

    if (outH == 0 || outW == 0)
        return Value::matrix(0, 0, Y.type(), mr);

    Value Ycrop = Value::matrix(outH, outW, Y.type(), mr);
    if (Y.type() == ValueType::COMPLEX) {
        const Complex *src = Y.complexData();
        Complex *dst       = Ycrop.complexDataMut();
        for (size_t c = 0; c < outW; ++c)
            for (size_t r = 0; r < outH; ++r)
                dst[c * outH + r] = src[(c + c0) * Hf + (r + r0)];
    } else {
        const double *src = Y.doubleData();
        double *dst       = Ycrop.doubleDataMut();
        for (size_t c = 0; c < outW; ++c)
            for (size_t r = 0; r < outH; ++r)
                dst[c * outH + r] = src[(c + c0) * Hf + (r + r0)];
    }
    return Ycrop;
}

Value psf2otf(const Value &PSF, Span<const size_t> outsize, std::pmr::memory_resource *mr)
{
    const auto &d = PSF.dims();
    const size_t inH = d.rows();
    const size_t inW = d.cols();
    const bool is1D = (inH == 1 || inW == 1);

    // Determine output size (default = input size).
    //   outsize as 2-vec [outH outW]  — applies for both 1-D and 2-D.
    //   outsize as scalar L           — for 1-D = new length; 2-D = L×L.
    size_t outH = inH;
    size_t outW = inW;
    if (outsize.size() >= 2) {
        outH = outsize[0];
        outW = outsize[1];
    } else if (outsize.size() == 1) {
        const size_t L = outsize[0];
        if (is1D) {
            if (inH == 1) { outH = 1; outW = L; }
            else          { outH = L; outW = 1; }
        } else { outH = L; outW = L; }
    }
    if (outH < inH || outW < inW)
        throw Error("psf2otf: OUTSIZE must be larger than PSF size",
                    0, 0, "psf2otf", "", "m:psf2otf:outsize");

    // Pad PSF with zeros (post-pad).
    Value padded;
    if (outH != inH || outW != inW) {
        padded = padarray(PSF, {(int)(outH - inH), (int)(outW - inW)}, PadMode::Constant, 0.0, "post", mr);
    } else {
        padded = PSF;
    }

    // Circular shift by -floor(insize / 2).
    const int64_t shiftR = -static_cast<int64_t>(inH / 2);
    const int64_t shiftC = -static_cast<int64_t>(inW / 2);
    Value shifted;
    if (is1D) {
        shifted = builtin::circshift(padded, (inH == 1) ? shiftC : shiftR, mr);
    } else {
        shifted = builtin::circshift(padded, shiftR, shiftC, mr);
    }

    // FFT.
    return is1D ? signal::fft(shifted, -1, 0, mr)
                : signal::fft2(shifted, -1, -1, mr);
}

Value otf2psf(const Value &OTF, Span<const size_t> outsize, std::pmr::memory_resource *mr)
{
    const auto &d = OTF.dims();
    const size_t inH = d.rows();
    const size_t inW = d.cols();
    const bool is1D = (inH == 1 || inW == 1);

    // Resolve target output shape.
    //   no outsize             → same as OTF (no cropping)
    //   1-element outsize      → 1-D vector of that length (matching the
    //                            non-singleton orientation), or L×L if
    //                            OTF is 2-D
    //   2-element outsize      → use directly
    size_t outH = inH;
    size_t outW = inW;
    if (outsize.size() == 1) {
        const size_t L = outsize[0];
        if (is1D) {
            if (inH == 1) { outH = 1; outW = L; }
            else          { outH = L; outW = 1; }
        } else { outH = L; outW = L; }
    } else if (outsize.size() >= 2) {
        outH = outsize[0];
        outW = outsize[1];
    }
    if (outH > inH || outW > inW)
        throw Error("otf2psf: OUTSIZE must not exceed the size of the OTF "
                    "array in any dimension",
                    0, 0, "otf2psf", "", "m:otf2psf:outsize");

    // Inverse FFT.
    Value psf = is1D ? signal::ifft(OTF, -1, 0, mr)
                     : signal::ifft2(OTF, -1, -1, mr);

    // Circular shift by +floor(OUTSIZE / 2), matching MATLAB's
    // otf2psf source (NOT floor(insize/2) — the shift amount depends
    // on the requested output size). With outsize == insize this
    // reduces to the standard "ifftshift" behaviour.
    const int64_t shiftR = static_cast<int64_t>(outH / 2);
    const int64_t shiftC = static_cast<int64_t>(outW / 2);
    Value shifted;
    if (is1D) {
        shifted = builtin::circshift(psf, (inH == 1) ? shiftC : shiftR, mr);
    } else {
        shifted = builtin::circshift(psf, shiftR, shiftC, mr);
    }

    // Crop to the requested outsize (top-left sub-matrix). After the
    // circshift the PSF support lands at the top-left corner.
    if (outH == inH && outW == inW) return shifted;

    const bool is_complex = shifted.isComplex();
    Value cropped = Value::matrix(outH, outW,
        is_complex ? ValueType::COMPLEX : ValueType::DOUBLE, mr);
    if (is_complex) {
        const Complex *src = shifted.complexData();
        Complex *dst = cropped.complexDataMut();
        for (size_t j = 0; j < outW; ++j)
            for (size_t i = 0; i < outH; ++i)
                dst[j * outH + i] = src[j * inH + i];
    } else {
        const double *src = shifted.doubleData();
        double *dst = cropped.doubleDataMut();
        for (size_t j = 0; j < outW; ++j)
            for (size_t i = 0; i < outH; ++i)
                dst[j * outH + i] = src[j * inH + i];
    }
    return cropped;
}

Value normxcorr2(const Value &templ, const Value &img, std::pmr::memory_resource *mr)
{
    const size_t mH = templ.dims().rows();
    const size_t mW = templ.dims().cols();
    const size_t bH = img.dims().rows();
    const size_t bW = img.dims().cols();
    const size_t mN = mH * mW;

    // Build:
    //   a = templ - mean(templ)  (centered template)
    //   b = img                  (uncentered — see note below)
    //
    // Note on centering: MATLAB's normxcorr2 uses LOCAL-mean centering
    // (per output position). Globally centering `b` and convolving with
    // a centered `a` does NOT match local-mean correlation when the
    // 'full' mode zero-pads out-of-bounds image regions: the implicit
    // zero pad inherits 0 instead of -mean_global, which biases corner
    // outputs by sign. Keeping `b` uncentered while `a` is centered
    // (Σ a = 0) gives the correct numerator: Σ img · a_centered, which
    // equals Σ (img − local_mean) · a_centered because local_mean · Σa
    // is zero. The denominator (sum_b_sq, sum_b) is invariant under
    // global centering, so we can also drop it there for clarity.
    Value a = Value::matrix(mH, mW, ValueType::DOUBLE, mr);
    Value b = Value::matrix(bH, bW, ValueType::DOUBLE, mr);
    double *ad = a.doubleDataMut();
    double *bd = b.doubleDataMut();

    long double sa = 0.0L;
    for (size_t i = 0; i < mN; ++i) sa += templ.elemAsDouble(i);
    const double ma = (mN > 0) ? static_cast<double>(sa / static_cast<long double>(mN)) : 0.0;
    for (size_t i = 0; i < mN; ++i)     ad[i] = templ.elemAsDouble(i) - ma;
    for (size_t i = 0; i < bH * bW; ++i) bd[i] = img.elemAsDouble(i);

    // Reversed template ar = rot180(a). Column-major: ar[(mW-1-c)*mH + (mH-1-r)] = a[c*mH + r].
    Value ar = Value::matrix(mH, mW, ValueType::DOUBLE, mr);
    double *ard = ar.doubleDataMut();
    for (size_t c = 0; c < mW; ++c)
        for (size_t r = 0; r < mH; ++r)
            ard[(mW - 1 - c) * mH + (mH - 1 - r)] = ad[c * mH + r];

    // Numerator: conv2(b, ar, 'full').
    Value c_num = signal::conv2(b, ar, "full", mr);

    // Denominator pieces use a1 = ones(size(a)).
    Value a1 = Value::matrix(mH, mW, ValueType::DOUBLE, mr);
    double *a1d = a1.doubleDataMut();
    for (size_t i = 0; i < mN; ++i) a1d[i] = 1.0;

    // b_sq = b .^ 2.
    Value b_sq = Value::matrix(bH, bW, ValueType::DOUBLE, mr);
    double *bsd = b_sq.doubleDataMut();
    for (size_t i = 0; i < bH * bW; ++i) bsd[i] = bd[i] * bd[i];

    Value sum_b_sq = signal::conv2(b_sq, a1, "full", mr);
    Value sum_b    = signal::conv2(b, a1, "full", mr);

    // c_denom = sum_b_sq - sum_b.^2 / mN (clamped at 0).
    const size_t outH = bH + mH - 1;
    const size_t outW = bW + mW - 1;
    Value c_denom = Value::matrix(outH, outW, ValueType::DOUBLE, mr);
    double *cdd = c_denom.doubleDataMut();
    const double *sbsd = sum_b_sq.doubleData();
    const double *sbd  = sum_b.doubleData();
    for (size_t i = 0; i < outH * outW; ++i) {
        double v = sbsd[i] - (sbd[i] * sbd[i]) /
                              static_cast<double>(mN > 0 ? mN : 1);
        if (v < 0.0) v = 0.0;
        cdd[i] = v;
    }

    // sumsq(a).
    long double sumsq_a = 0.0L;
    for (size_t i = 0; i < mN; ++i) sumsq_a += static_cast<long double>(ad[i]) * ad[i];
    const double sa2 = static_cast<double>(sumsq_a);

    // c = c_num / sqrt(c_denom * sumsq_a); inf/nan → 0.
    Value c_out = Value::matrix(outH, outW, ValueType::DOUBLE, mr);
    double *cod = c_out.doubleDataMut();
    const double *cnd = c_num.doubleData();
    for (size_t i = 0; i < outH * outW; ++i) {
        const double denom = std::sqrt(cdd[i] * sa2);
        double v = (denom > 0.0) ? cnd[i] / denom : 0.0;
        if (!std::isfinite(v)) v = 0.0;
        cod[i] = v;
    }
    return c_out;
}

Value checkerboard(size_t side, size_t M, size_t N, std::pmr::memory_resource *mr)
{
    const size_t H = 2 * M * side;
    const size_t W = 2 * N * side;
    Value out = Value::matrix(H, W, ValueType::DOUBLE, mr);
    if (H == 0 || W == 0 || side == 0) return out;
    double *od = out.doubleDataMut();

    // Tile pattern: 2*side × 2*side. Build via linspace(-1, 1, 2*side):
    //   x[i] = -1 + 2 * i / (2*side - 1)
    // tile(r, c) = (x[r] * x[c]) < 0  → 1.0 in opposite-sign quadrants.
    const size_t S2 = 2 * side;
    std::vector<double> x(S2);
    if (S2 == 1) x[0] = -1.0;
    else
        for (size_t i = 0; i < S2; ++i)
            x[i] = -1.0 + 2.0 * static_cast<double>(i)
                          / static_cast<double>(S2 - 1);

    // Right half (cols ≥ W/2) is dimmed to 0.7.
    const size_t halfW = W / 2;
    for (size_t c = 0; c < W; ++c) {
        const double xc = x[c % S2];
        const double dim = (c >= halfW) ? 0.7 : 1.0;
        for (size_t r = 0; r < H; ++r) {
            const double xr = x[r % S2];
            const double tile = (xr * xc < 0.0) ? 1.0 : 0.0;
            od[c * H + r] = tile * dim;
        }
    }
    return out;
}

// ── deconvwnr (Wiener inverse filter) ──────────────────────────────
//
// J = ifft( conj(H) * S_x / (|H|^2 * S_x + S_u) * fft(I) )
// where H = psf2otf(PSF, size(I)); S_u is noise power spectrum and
// S_x is signal power spectrum. Scalar NSR = S_u/S_x maps to S_u =
// NSR, S_x = 1. Scalar NCORR/ICORR maps to S_u=NCORR, S_x=ICORR.
//
// Algorithm transliterated verbatim from MATLAB R2025b deconvwnr.m
// (see Gonzalez & Woods, *Digital Image Processing*, 2e § 5.8).
namespace {

// Wide cast input I to DOUBLE, recording the source class for the
// later quantisation cast on output.
Value to_double_pmr(const Value &I, std::pmr::memory_resource *mr)
{
    const auto &d = I.dims();
    const std::size_t H = d.rows();
    const std::size_t W = d.cols();
    Value out = d.is3D()
        ? Value::matrix3d(H, W, d.pages(), ValueType::DOUBLE, mr)
        : Value::matrix(H, W, ValueType::DOUBLE, mr);
    double *od = out.doubleDataMut();
    const std::size_t N = I.numel();
    switch (I.type()) {
        case ValueType::DOUBLE:
            std::memcpy(od, I.doubleData(), N * sizeof(double)); break;
        case ValueType::SINGLE:
            for (std::size_t i = 0; i < N; ++i) od[i] = I.singleData()[i]; break;
        case ValueType::UINT8:
            for (std::size_t i = 0; i < N; ++i) od[i] = I.uint8Data()[i] / 255.0; break;
        case ValueType::UINT16:
            for (std::size_t i = 0; i < N; ++i) od[i] = I.uint16Data()[i] / 65535.0; break;
        case ValueType::INT16:
            for (std::size_t i = 0; i < N; ++i)
                od[i] = (I.int16Data()[i] + 32768.0) / 65535.0; break;
        default:
            throw Error("deconvwnr: unsupported image class",
                        0, 0, "deconvwnr", "", "m:deconvwnr:cls");
    }
    return out;
}

Value real_back_to_class(const Value &Jd, ValueType outT,
                         std::pmr::memory_resource *mr)
{
    const auto &d = Jd.dims();
    Value out = d.is3D()
        ? Value::matrix3d(d.rows(), d.cols(), d.pages(), outT, mr)
        : Value::matrix(d.rows(), d.cols(), outT, mr);
    const std::size_t N = Jd.numel();
    const double *src = Jd.doubleData();
    auto sat_u8  = [](double v) {
        if (v <= 0.0) return uint8_t{0};
        if (v >= 1.0) return uint8_t{255};
        return static_cast<uint8_t>(std::lround(v * 255.0));
    };
    auto sat_u16 = [](double v) {
        if (v <= 0.0) return uint16_t{0};
        if (v >= 1.0) return uint16_t{65535};
        return static_cast<uint16_t>(std::lround(v * 65535.0));
    };
    auto sat_i16 = [](double v) {
        if (v <= 0.0) return int16_t{-32768};
        if (v >= 1.0) return int16_t{32767};
        return static_cast<int16_t>(std::lround(v * 65535.0 - 32768.0));
    };
    switch (outT) {
        case ValueType::DOUBLE:
            std::memcpy(out.doubleDataMut(), src, N * sizeof(double)); break;
        case ValueType::SINGLE:
            for (std::size_t i = 0; i < N; ++i)
                out.singleDataMut()[i] = static_cast<float>(src[i]); break;
        case ValueType::UINT8:
            for (std::size_t i = 0; i < N; ++i)
                out.uint8DataMut()[i] = sat_u8(src[i]); break;
        case ValueType::UINT16:
            for (std::size_t i = 0; i < N; ++i)
                out.uint16DataMut()[i] = sat_u16(src[i]); break;
        case ValueType::INT16:
            for (std::size_t i = 0; i < N; ++i)
                out.int16DataMut()[i] = sat_i16(src[i]); break;
        default:
            throw Error("deconvwnr: unsupported output class",
                        0, 0, "deconvwnr", "", "m:deconvwnr:cls");
    }
    return out;
}

// Core impl: takes I (already DOUBLE), PSF (DOUBLE), and power-
// spectrum scalars S_u and S_x (after the array case has been
// reduced via fftn-and-abs). Returns DOUBLE J of size(I).
Value deconvwnr_core_scalar_uxsx(const Value &I, const Value &PSF,
                                 double S_u, double S_x,
                                 std::pmr::memory_resource *mr)
{
    const auto &dI = I.dims();
    const std::size_t H = dI.rows();
    const std::size_t W = dI.cols();
    const bool is3 = dI.is3D();
    const std::size_t P = is3 ? dI.pages() : 1;

    // 2-D OTF for now (deconvwnr handles N-D in MATLAB; we cover
    // 2-D and 3-D via per-page processing — common in HDR / volumes).
    std::array<std::size_t, 2> outsize{H, W};
    Value Hf = psf2otf(PSF, Span<const std::size_t>(outsize.data(), 2), mr);
    // psf2otf may return DOUBLE for real-symmetric PSFs; normalise
    // to a COMPLEX buffer so the rest of this function can access
    // both real and imag uniformly.
    const std::size_t plane = H * W;
    std::pmr::vector<Complex> Hcplx(plane, Complex{0, 0}, mr);
    if (Hf.isComplex()) {
        const Complex *src = Hf.complexData();
        for (std::size_t i = 0; i < plane; ++i) Hcplx[i] = src[i];
    } else {
        for (std::size_t i = 0; i < plane; ++i)
            Hcplx[i] = Complex{Hf.elemAsDouble(i), 0.0};
    }
    const Complex *Hd = Hcplx.data();
    // denom = |H|^2 * S_x + S_u, clamped at sqrt(eps).
    Value denom = Value::matrix(H, W, ValueType::DOUBLE, mr);
    double *denomD = denom.doubleDataMut();
    const double sqrt_eps = std::sqrt(std::numeric_limits<double>::epsilon());
    for (std::size_t i = 0; i < plane; ++i) {
        const double m2 = Hd[i].real() * Hd[i].real() + Hd[i].imag() * Hd[i].imag();
        double d = m2 * S_x + S_u;
        if (d < sqrt_eps) d = sqrt_eps;
        denomD[i] = d;
    }

    // Process each page through the same OTF.
    Value J = is3
        ? Value::matrix3d(H, W, P, ValueType::DOUBLE, mr)
        : Value::matrix(H, W, ValueType::DOUBLE, mr);

    for (std::size_t pg = 0; pg < P; ++pg) {
        Value Ip = is3 ? Value::matrix(H, W, ValueType::DOUBLE, mr) : I;
        if (is3) {
            double *ipd = Ip.doubleDataMut();
            for (std::size_t i = 0; i < plane; ++i)
                ipd[i] = I.doubleData()[pg * plane + i];
        }
        Value FI = signal::fft2(Ip, -1, -1, mr);
        // FFT2 may collapse to DOUBLE when input is real-symmetric.
        std::pmr::vector<Complex> FIcplx(plane, Complex{0, 0}, mr);
        if (FI.isComplex()) {
            const Complex *src = FI.complexData();
            for (std::size_t i = 0; i < plane; ++i) FIcplx[i] = src[i];
        } else {
            for (std::size_t i = 0; i < plane; ++i)
                FIcplx[i] = Complex{FI.elemAsDouble(i), 0.0};
        }
        Value FG = Value::matrix(H, W, ValueType::COMPLEX, mr);
        Complex *fg = FG.complexDataMut();
        const Complex *fi = FIcplx.data();
        for (std::size_t i = 0; i < plane; ++i) {
            // G(k) = conj(H(k)) * S_x / denom(k); multiply by FI(k).
            const Complex Hk = Hd[i];
            const Complex Hc{Hk.real(), -Hk.imag()};
            const double g_scale = S_x / denomD[i];
            const Complex Gk{Hc.real() * g_scale, Hc.imag() * g_scale};
            fg[i] = Complex{Gk.real() * fi[i].real() - Gk.imag() * fi[i].imag(),
                            Gk.real() * fi[i].imag() + Gk.imag() * fi[i].real()};
        }
        Value Jp = signal::ifft2(FG, -1, -1, mr);
        // IFFT2 may also collapse to DOUBLE for purely-real spectra.
        auto jp_at = [&](std::size_t i) -> double {
            if (Jp.isComplex()) return Jp.complexData()[i].real();
            return Jp.elemAsDouble(i);
        };
        if (is3) {
            double *jod = J.doubleDataMut();
            for (std::size_t i = 0; i < plane; ++i)
                jod[pg * plane + i] = jp_at(i);
        } else {
            double *jod = J.doubleDataMut();
            for (std::size_t i = 0; i < plane; ++i)
                jod[i] = jp_at(i);
        }
    }
    return J;
}

} // anonymous

Value deconvwnr(const Value &I, const Value &PSF, double nsr,
                std::pmr::memory_resource *mr)
{
    if (PSF.dims().is3D())
        throw Error("deconvwnr: PSF must be 2-D",
                    0, 0, "deconvwnr", "", "m:deconvwnr:psf");
    const ValueType inT = I.type();
    Value Id = to_double_pmr(I, mr);
    Value PSFd = to_double_pmr(PSF, mr);
    Value J = deconvwnr_core_scalar_uxsx(Id, PSFd, nsr, 1.0, mr);
    if (inT == ValueType::DOUBLE) return J;
    return real_back_to_class(J, inT, mr);
}

Value deconvwnr(const Value &I, const Value &PSF,
                const Value &ncorr, const Value &icorr,
                std::pmr::memory_resource *mr)
{
    if (PSF.dims().is3D())
        throw Error("deconvwnr: PSF must be 2-D",
                    0, 0, "deconvwnr", "", "m:deconvwnr:psf");
    const ValueType inT = I.type();
    Value Id = to_double_pmr(I, mr);
    Value PSFd = to_double_pmr(PSF, mr);

    // Determine S_u and S_x — scalars or same-size power spectra
    // (we take |fftn(ACF, sizeI)| per MATLAB's powerSpectrumFromACF).
    const auto &dI = Id.dims();
    const std::size_t H = dI.rows();
    const std::size_t W = dI.cols();
    const std::size_t plane = H * W;

    auto build_S = [&](const Value &acf, const char *name) -> std::pair<bool, std::pmr::vector<double>> {
        if (acf.numel() == 1) {
            // Scalar: uniform spectrum equal to that value.
            std::pmr::vector<double> v(mr);
            v.push_back(acf.toScalar());
            return {true, std::move(v)};
        }
        if (acf.dims().rows() == H && acf.dims().cols() == W
            && !acf.dims().is3D()) {
            // Same-size ACF: |fft2(acf)|
            Value F = signal::fft2(acf, -1, -1, mr);
            std::pmr::vector<double> S(plane, 0.0, mr);
            const Complex *fc = F.complexData();
            for (std::size_t i = 0; i < plane; ++i) {
                const double m = std::sqrt(fc[i].real() * fc[i].real()
                                         + fc[i].imag() * fc[i].imag());
                S[i] = m;
            }
            return {false, std::move(S)};
        }
        throw Error(std::string("deconvwnr: ") + name
                  + " must be a scalar or same size as I "
                    "(1-D extrapolation form not supported)",
                    0, 0, "deconvwnr", "", "m:deconvwnr:acf");
    };

    auto [su_scalar, S_u] = build_S(ncorr, "NCORR");
    auto [sx_scalar, S_x] = build_S(icorr, "ICORR");

    Value J;
    if (su_scalar && sx_scalar) {
        J = deconvwnr_core_scalar_uxsx(Id, PSFd, S_u[0], S_x[0], mr);
    } else {
        // Non-scalar spectra: apply per pixel inside the loop. Reuse
        // the scalar-core's H computation, then redo the denom build.
        const bool is3 = dI.is3D();
        const std::size_t P = is3 ? dI.pages() : 1;
        std::array<std::size_t, 2> outsize{H, W};
        Value Hf = psf2otf(PSFd, Span<const std::size_t>(outsize.data(), 2), mr);
        // Same DOUBLE-vs-COMPLEX normalisation as the scalar path.
        std::pmr::vector<Complex> Hcplx(plane, Complex{0, 0}, mr);
        if (Hf.isComplex()) {
            const Complex *src = Hf.complexData();
            for (std::size_t i = 0; i < plane; ++i) Hcplx[i] = src[i];
        } else {
            for (std::size_t i = 0; i < plane; ++i)
                Hcplx[i] = Complex{Hf.elemAsDouble(i), 0.0};
        }
        const Complex *Hd = Hcplx.data();
        Value denom = Value::matrix(H, W, ValueType::DOUBLE, mr);
        double *denomD = denom.doubleDataMut();
        const double sqrt_eps = std::sqrt(std::numeric_limits<double>::epsilon());
        auto get_S = [&](const std::pmr::vector<double> &S, std::size_t i, bool scalar) {
            return scalar ? S[0] : S[i];
        };
        for (std::size_t i = 0; i < plane; ++i) {
            const double m2 = Hd[i].real() * Hd[i].real()
                            + Hd[i].imag() * Hd[i].imag();
            double d = m2 * get_S(S_x, i, sx_scalar) + get_S(S_u, i, su_scalar);
            if (d < sqrt_eps) d = sqrt_eps;
            denomD[i] = d;
        }

        J = is3 ? Value::matrix3d(H, W, P, ValueType::DOUBLE, mr)
                 : Value::matrix(H, W, ValueType::DOUBLE, mr);
        for (std::size_t pg = 0; pg < P; ++pg) {
            Value Ip = is3 ? Value::matrix(H, W, ValueType::DOUBLE, mr) : Id;
            if (is3) {
                double *ipd = Ip.doubleDataMut();
                for (std::size_t i = 0; i < plane; ++i)
                    ipd[i] = Id.doubleData()[pg * plane + i];
            }
            Value FI = signal::fft2(Ip, -1, -1, mr);
            std::pmr::vector<Complex> FIcplx(plane, Complex{0, 0}, mr);
            if (FI.isComplex()) {
                const Complex *src = FI.complexData();
                for (std::size_t i = 0; i < plane; ++i) FIcplx[i] = src[i];
            } else {
                for (std::size_t i = 0; i < plane; ++i)
                    FIcplx[i] = Complex{FI.elemAsDouble(i), 0.0};
            }
            Value FG = Value::matrix(H, W, ValueType::COMPLEX, mr);
            Complex *fg = FG.complexDataMut();
            const Complex *fi = FIcplx.data();
            for (std::size_t i = 0; i < plane; ++i) {
                const Complex Hk = Hd[i];
                const Complex Hc{Hk.real(), -Hk.imag()};
                const double sx_i = get_S(S_x, i, sx_scalar);
                const double g_scale = sx_i / denomD[i];
                const Complex Gk{Hc.real() * g_scale, Hc.imag() * g_scale};
                fg[i] = Complex{Gk.real() * fi[i].real() - Gk.imag() * fi[i].imag(),
                                Gk.real() * fi[i].imag() + Gk.imag() * fi[i].real()};
            }
            Value Jp = signal::ifft2(FG, -1, -1, mr);
            auto jp_at = [&](std::size_t i) -> double {
                if (Jp.isComplex()) return Jp.complexData()[i].real();
                return Jp.elemAsDouble(i);
            };
            if (is3) {
                double *jod = J.doubleDataMut();
                for (std::size_t i = 0; i < plane; ++i)
                    jod[pg * plane + i] = jp_at(i);
            } else {
                double *jod = J.doubleDataMut();
                for (std::size_t i = 0; i < plane; ++i) jod[i] = jp_at(i);
            }
        }
    }
    if (inT == ValueType::DOUBLE) return J;
    return real_back_to_class(J, inT, mr);
}

// ── edgetaper (image-edge taper for FFT-based deblur) ─────────────
//
// MATLAB R2025b edgetaper.m algorithm:
//
//   For each non-singleton PSF dim n:
//     proj   = sum(PSF over all other dims)
//     beta_n = autocorr of proj at length sizeI(n) - 1, normalised,
//              padded symmetrically to length sizeI(n).
//   alpha = outer product of (1 - beta_n) across all NS dims.
//   blurredI = real(ifftn(fftn(I) .* psf2otf(PSF, sizeI)))
//   J = alpha .* I + (1 - alpha) .* blurredI, clipped to [min(I), max(I)]
//   cast back to class(I).
//
// We restrict to 2-D inputs (the most common case); MATLAB extends
// the same algorithm to N-D, but the 3-D-and-up form is rarely
// used. A 3-D input throws a clear "use slice + loop" error.
Value edgetaper(const Value &I, const Value &PSF, std::pmr::memory_resource *mr)
{
    const auto &dI   = I.dims();
    const auto &dPSF = PSF.dims();
    if (dI.is3D())
        throw Error("edgetaper: I must be 2-D (slice 3-D inputs and call "
                    "per page)",
                    0, 0, "edgetaper", "", "m:edgetaper:rank");
    if (dPSF.is3D())
        throw Error("edgetaper: PSF must be 2-D",
                    0, 0, "edgetaper", "", "m:edgetaper:psfRank");
    const std::size_t H  = dI.rows();
    const std::size_t W  = dI.cols();
    const std::size_t PH = dPSF.rows();
    const std::size_t PW = dPSF.cols();
    if (H * W < 2)
        throw Error("edgetaper: I must have at least 2 elements",
                    0, 0, "edgetaper", "", "m:edgetaper:tooSmall");
    if (PH * PW < 2)
        throw Error("edgetaper: PSF must have at least 2 elements",
                    0, 0, "edgetaper", "", "m:edgetaper:psfTooSmall");
    if (2 * PH > H || 2 * PW > W)
        throw Error("edgetaper: PSF size must be smaller than half of "
                    "the image size in any nonsingleton dimension",
                    0, 0, "edgetaper", "", "m:edgetaper:psfSize");

    // Promote to DOUBLE for the math (we round back to class at the end).
    const ValueType inT = I.type();
    Value Id   = to_double_pmr(I, mr);
    Value PSFd = to_double_pmr(PSF, mr);
    const double *psfd = PSFd.doubleData();

    // Track input range for the final clip.
    double Imin =  std::numeric_limits<double>::infinity();
    double Imax = -std::numeric_limits<double>::infinity();
    {
        const std::size_t N = Id.numel();
        const double *id = Id.doubleData();
        for (std::size_t k = 0; k < N; ++k) {
            if (id[k] < Imin) Imin = id[k];
            if (id[k] > Imax) Imax = id[k];
        }
    }

    auto compute_beta = [&](std::size_t dim, std::size_t sizeI_n) {
        // proj over the OTHER dim of PSF.
        std::pmr::vector<double> proj(mr);
        if (dim == 0) {
            // sum along columns (per row).
            proj.resize(PH, 0.0);
            for (std::size_t c = 0; c < PW; ++c)
                for (std::size_t r = 0; r < PH; ++r)
                    proj[r] += psfd[c * PH + r];
        } else {
            // dim == 1: sum along rows (per col).
            proj.resize(PW, 0.0);
            for (std::size_t c = 0; c < PW; ++c)
                for (std::size_t r = 0; r < PH; ++r)
                    proj[c] += psfd[c * PH + r];
        }
        // fft length = sizeI(n) - 1.
        const std::size_t L = sizeI_n - 1;
        Value vec = Value::matrix(1, L, ValueType::DOUBLE, mr);
        double *vd = vec.doubleDataMut();
        for (std::size_t k = 0; k < L; ++k)
            vd[k] = (k < proj.size()) ? proj[k] : 0.0;
        // F = fft(proj_padded) — dim=0 → first non-singleton (cols of 1×L row).
        Value F = signal::fft(vec, -1, 0, mr);
        std::pmr::vector<double> Zsq(L, 0.0, mr);
        if (F.isComplex()) {
            const Complex *fc = F.complexData();
            for (std::size_t k = 0; k < L; ++k)
                Zsq[k] = fc[k].real() * fc[k].real()
                       + fc[k].imag() * fc[k].imag();
        } else {
            for (std::size_t k = 0; k < L; ++k) {
                const double v = F.elemAsDouble(k);
                Zsq[k] = v * v;
            }
        }
        // ifft(|F|^2) — real autocorrelation.
        Value Zv = Value::matrix(1, L, ValueType::DOUBLE, mr);
        for (std::size_t k = 0; k < L; ++k) Zv.doubleDataMut()[k] = Zsq[k];
        Value Z = signal::ifft(Zv, -1, 0, mr);
        std::pmr::vector<double> zr(L, 0.0, mr);
        if (Z.isComplex()) {
            const Complex *zc = Z.complexData();
            for (std::size_t k = 0; k < L; ++k) zr[k] = zc[k].real();
        } else {
            for (std::size_t k = 0; k < L; ++k) zr[k] = Z.elemAsDouble(k);
        }
        // Make symmetric: append first element.
        zr.push_back(zr.front());
        // Normalise by max.
        double zmax = 0.0;
        for (double v : zr) if (v > zmax) zmax = v;
        if (zmax > 0.0) for (auto &v : zr) v /= zmax;
        return zr;       // length sizeI_n
    };

    std::pmr::vector<double> beta_r = compute_beta(0, H);
    std::pmr::vector<double> beta_c = compute_beta(1, W);

    // alpha = (1 - beta_r) * (1 - beta_c)'  — H × W outer product.
    Value alpha = Value::matrix(H, W, ValueType::DOUBLE, mr);
    double *ad = alpha.doubleDataMut();
    for (std::size_t c = 0; c < W; ++c)
        for (std::size_t r = 0; r < H; ++r)
            ad[c * H + r] = (1.0 - beta_r[r]) * (1.0 - beta_c[c]);

    // blurredI = real(ifft2(fft2(I) .* psf2otf(PSF, sizeI))).
    std::array<std::size_t, 2> outsize{H, W};
    Value Hotf = psf2otf(PSFd, Span<const std::size_t>(outsize.data(), 2), mr);
    const std::size_t plane = H * W;
    std::pmr::vector<Complex> Hc(plane, Complex{0, 0}, mr);
    if (Hotf.isComplex()) {
        const Complex *src = Hotf.complexData();
        for (std::size_t i = 0; i < plane; ++i) Hc[i] = src[i];
    } else {
        for (std::size_t i = 0; i < plane; ++i)
            Hc[i] = Complex{Hotf.elemAsDouble(i), 0.0};
    }
    Value FI = signal::fft2(Id, -1, -1, mr);
    std::pmr::vector<Complex> Fic(plane, Complex{0, 0}, mr);
    if (FI.isComplex()) {
        const Complex *src = FI.complexData();
        for (std::size_t i = 0; i < plane; ++i) Fic[i] = src[i];
    } else {
        for (std::size_t i = 0; i < plane; ++i)
            Fic[i] = Complex{FI.elemAsDouble(i), 0.0};
    }
    Value FG = Value::matrix(H, W, ValueType::COMPLEX, mr);
    Complex *fg = FG.complexDataMut();
    for (std::size_t i = 0; i < plane; ++i)
        fg[i] = Complex{Fic[i].real() * Hc[i].real() - Fic[i].imag() * Hc[i].imag(),
                        Fic[i].real() * Hc[i].imag() + Fic[i].imag() * Hc[i].real()};
    Value Blur = signal::ifft2(FG, -1, -1, mr);
    auto blur_at = [&](std::size_t i) -> double {
        if (Blur.isComplex()) return Blur.complexData()[i].real();
        return Blur.elemAsDouble(i);
    };

    // J = alpha .* I + (1 - alpha) .* blurredI, clipped to [Imin, Imax].
    Value Jd = Value::matrix(H, W, ValueType::DOUBLE, mr);
    double *jd = Jd.doubleDataMut();
    const double *id = Id.doubleData();
    for (std::size_t i = 0; i < plane; ++i) {
        const double a = ad[i];
        double v = a * id[i] + (1.0 - a) * blur_at(i);
        if (v < Imin) v = Imin;
        if (v > Imax) v = Imax;
        jd[i] = v;
    }

    if (inT == ValueType::DOUBLE || inT == ValueType::SINGLE) {
        if (inT == ValueType::DOUBLE) return Jd;
        // SINGLE: cast back.
        Value out = Value::matrix(H, W, ValueType::SINGLE, mr);
        for (std::size_t i = 0; i < plane; ++i)
            out.singleDataMut()[i] = static_cast<float>(jd[i]);
        return out;
    }
    return real_back_to_class(Jd, inT, mr);
}

namespace detail {

void integralImage_reg(Span<const Value> args, size_t /*nargout*/,
                       Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("integralImage: requires (I)",
                    0, 0, "integralImage", "", "m:integralImage:nargin");
    outs[0] = integralImage(args[0], ctx.engine->resource());
}

void integralImage3_reg(Span<const Value> args, size_t /*nargout*/,
                        Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("integralImage3: requires (V)",
                    0, 0, "integralImage3", "", "m:integralImage3:nargin");
    outs[0] = integralImage3(args[0], ctx.engine->resource());
}

void dct2_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
              CallContext &ctx)
{
    if (args.empty())
        throw Error("dct2: requires 1 argument",
                    0, 0, "dct2", "", "m:dct2:nargin");
    outs[0] = dct2(args[0], ctx.engine->resource());
}

void idct2_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
               CallContext &ctx)
{
    if (args.empty())
        throw Error("idct2: requires 1 argument",
                    0, 0, "idct2", "", "m:idct2:nargin");
    outs[0] = idct2(args[0], ctx.engine->resource());
}

void dctmtx_reg(Span<const Value> args, size_t /*nargout*/, Span<Value> outs,
                CallContext &ctx)
{
    if (args.empty())
        throw Error("dctmtx: requires 1 argument (N)",
                    0, 0, "dctmtx", "", "m:dctmtx:nargin");
    outs[0] = dctmtx(args[0].toScalar(), ctx.engine->resource());
}

void phantom_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    Value model_or_E;
    size_t n = 0;
    // Argument forms:
    //   phantom()                            -> defaults
    //   phantom(model_str | E)               -> single arg
    //   phantom(N)                           -> single numeric scalar
    //   phantom(model_str | E, N)            -> two args
    if (args.size() == 1) {
        const Value &a = args[0];
        if (!a.isEmpty() && (a.isChar() || a.isString() || a.numel() != 1))
            model_or_E = a;
        else if (!a.isEmpty())
            n = static_cast<size_t>(a.toScalar());
    } else if (args.size() >= 2) {
        if (!args[0].isEmpty()) model_or_E = args[0];
        if (!args[1].isEmpty()) n = static_cast<size_t>(args[1].toScalar());
    }
    auto [head, E] = phantom(model_or_E, n, mr);
    outs[0] = std::move(head);
    if (nargout > 1) outs[1] = std::move(E);
}

void normxcorr2_reg(Span<const Value> args, size_t /*nargout*/,
                    Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("normxcorr2: requires (template, img)",
                    0, 0, "normxcorr2", "", "m:normxcorr2:nargin");
    outs[0] = normxcorr2(args[0], args[1], ctx.engine->resource());
}

void psf2otf_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("psf2otf: requires (PSF [, outsize])",
                    0, 0, "psf2otf", "", "m:psf2otf:nargin");
    auto *mr = ctx.engine->resource();
    ScratchArena scratch(mr);
    ScratchVec<size_t> outsizeBuf(&scratch);
    if (args.size() >= 2 && !args[1].isEmpty()) {
        const size_t n = args[1].numel();
        outsizeBuf.reserve(n);
        for (size_t i = 0; i < n; ++i)
            outsizeBuf.push_back(static_cast<size_t>(args[1].elemAsDouble(i)));
    }
    outs[0] = psf2otf(args[0], Span<const size_t>(outsizeBuf.data(), outsizeBuf.size()), mr);
}

void bestblk_reg(Span<const Value> args, size_t nargout,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("bestblk: requires (IMS [, k])",
                    0, 0, "bestblk", "", "m:bestblk:nargin");
    auto *mr = ctx.engine->resource();
    const double k = (args.size() >= 2 && !args[1].isEmpty())
                       ? args[1].toScalar() : 100.0;
    Value v = bestblk(args[0], k, mr);
    if (nargout <= 1) { outs[0] = std::move(v); return; }
    // Multi-output form: split row vector into scalars.
    const size_t nd = v.numel();
    const size_t M = std::min<size_t>(nargout, nd);
    for (size_t i = 0; i < M; ++i) {
        outs[i] = Value::scalar(v.elemAsDouble(i), mr);
    }
}

void fftconv2_reg(Span<const Value> args, size_t /*nargout*/,
                  Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("fftconv2: requires (A, B [, shape])",
                    0, 0, "fftconv2", "", "m:fftconv2:nargin");
    std::string shape = "full";
    if (args.size() >= 3 && !args[2].isEmpty()) {
        if (!args[2].isChar() && !args[2].isString())
            throw Error("fftconv2: shape must be a string",
                        0, 0, "fftconv2", "", "m:fftconv2:shape");
        shape = args[2].toString();
    }
    outs[0] = fftconv2(args[0], args[1], shape, ctx.engine->resource());
}

void edgetaper_reg(Span<const Value> args, std::size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("edgetaper: requires (I, PSF)",
                    0, 0, "edgetaper", "", "m:edgetaper:nargin");
    outs[0] = edgetaper(args[0], args[1], ctx.engine->resource());
}

void deconvwnr_reg(Span<const Value> args, std::size_t /*nargout*/,
                   Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("deconvwnr: requires (I, PSF [, NSR | NCORR, ICORR])",
                    0, 0, "deconvwnr", "", "m:deconvwnr:nargin");
    auto *mr = ctx.engine->resource();
    if (args.size() == 2) {
        outs[0] = deconvwnr(args[0], args[1], 0.0, mr);
    } else if (args.size() == 3) {
        // 3-arg form: scalar NSR (most common case).
        if (args[2].numel() != 1)
            throw Error("deconvwnr: 3-arg NSR must be a scalar; use the "
                        "4-arg (NCORR, ICORR) form for array spectra",
                        0, 0, "deconvwnr", "", "m:deconvwnr:nsr");
        outs[0] = deconvwnr(args[0], args[1], args[2].toScalar(), mr);
    } else {
        outs[0] = deconvwnr(args[0], args[1], args[2], args[3], mr);
    }
}

void otf2psf_reg(Span<const Value> args, size_t /*nargout*/,
                 Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("otf2psf: requires (OTF [, outsize])",
                    0, 0, "otf2psf", "", "m:otf2psf:nargin");
    auto *mr = ctx.engine->resource();
    ScratchArena scratch(mr);
    ScratchVec<size_t> outsizeBuf(&scratch);
    if (args.size() >= 2 && !args[1].isEmpty()) {
        const size_t n = args[1].numel();
        outsizeBuf.reserve(n);
        for (size_t i = 0; i < n; ++i)
            outsizeBuf.push_back(static_cast<size_t>(args[1].elemAsDouble(i)));
    }
    outs[0] = otf2psf(args[0], Span<const size_t>(outsizeBuf.data(), outsizeBuf.size()), mr);
}

void checkerboard_reg(Span<const Value> args, size_t /*nargout*/,
                      Span<Value> outs, CallContext &ctx)
{
    size_t side = 10, M = 4, N = 4;
    if (args.size() >= 1 && !args[0].isEmpty()) {
        const double s = args[0].toScalar();
        if (s < 0.0 || s != std::floor(s))
            throw Error("checkerboard: SIDE must be a non-negative integer",
                        0, 0, "checkerboard", "", "m:checkerboard:side");
        side = static_cast<size_t>(s);
    }
    if (args.size() >= 2 && !args[1].isEmpty()) {
        const Value &v = args[1];
        if (v.numel() == 1) { M = N = static_cast<size_t>(v.toScalar()); }
        else if (v.numel() >= 2) {
            M = static_cast<size_t>(v.elemAsDouble(0));
            N = static_cast<size_t>(v.elemAsDouble(1));
        }
    }
    if (args.size() >= 3 && !args[2].isEmpty())
        N = static_cast<size_t>(args[2].toScalar());
    outs[0] = checkerboard(side, M, N, ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::image
