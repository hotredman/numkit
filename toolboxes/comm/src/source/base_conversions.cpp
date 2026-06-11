// toolboxes/comm/src/source/base_conversions.cpp
//
// Communications Toolbox base-conversion utilities:
//
//   bit2int(b, n [, msbfirst])  Pack column-vector of bits into integers
//                                (n bits per int). Default msbfirst=true.
//   int2bit(d, n [, msbfirst])  Inverse: unpack integers into n-bit groups
//   bi2de(b [, base [, msbf]])  Legacy synonym (rows = numbers, default
//                                LSB-first / 'right-msb'). Optional base.
//   de2bi(d [, n [, base [, msbf]]])  Legacy inverse: rows = digit groups
//   vec2mat(v, n [, padval])    Reshape vector into N-column matrix with
//                                padding (default 0). Row-major fill.
//                                Two outputs: [m, padded] (count of pad
//                                values added to fill the last row).
//
// PMR HARD RULE: every fn takes std::pmr::memory_resource *mr.
// Scratch via ScratchArena/ScratchVec. No std::vector for transient
// buffers.

#include <numkit/comm/source/base_conversions.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string>

namespace numkit::comm {

namespace {

// Read element as integer (truncating).
inline int64_t getI64(const Value &v, size_t i)
{
    return static_cast<int64_t>(v.elemAsDouble(i));
}

} // namespace

// ── bit2int ───────────────────────────────────────────────────────────
// Pack column-vector of bits (length divisible by n) into an integer
// vector of length numel(b) / n. MSB-first by default.
Value bit2int(const Value &b, int n, bool msbfirst,
              std::pmr::memory_resource *mr)
{
    if (n <= 0 || n > 64)
        throw Error("bit2int: n must be in 1..64",
                    0, 0, "bit2int", "", "numkit:bit2int:BadN");
    const size_t L = b.numel();
    if (L % static_cast<size_t>(n) != 0)
        throw Error("bit2int: numel(b) must be divisible by n",
                    0, 0, "bit2int", "", "numkit:bit2int:BadLen");
    const size_t M = L / static_cast<size_t>(n);
    Value out = Value::matrix(M, 1, ValueType::DOUBLE, mr);
    if (M == 0) return out;
    double *od = out.doubleDataMut();
    for (size_t k = 0; k < M; ++k) {
        uint64_t v = 0;
        for (int j = 0; j < n; ++j) {
            const size_t bitIdx = k * n + (msbfirst ? j : (n - 1 - j));
            v = (v << 1) | (getI64(b, bitIdx) & 1);
        }
        od[k] = static_cast<double>(v);
    }
    return out;
}

// ── int2bit ───────────────────────────────────────────────────────────
// Inverse of bit2int. d is a row/col vector of integers; returns
// (n × numel(d)) bit matrix (each input contributes one column of n bits).
Value int2bit(const Value &d, int n, bool msbfirst,
              std::pmr::memory_resource *mr)
{
    if (n <= 0 || n > 64)
        throw Error("int2bit: n must be in 1..64",
                    0, 0, "int2bit", "", "numkit:int2bit:BadN");
    const size_t M = d.numel();
    Value out = Value::matrix(static_cast<size_t>(n), M, ValueType::DOUBLE, mr);
    if (M == 0) return out;
    double *od = out.doubleDataMut();
    for (size_t k = 0; k < M; ++k) {
        const uint64_t v = static_cast<uint64_t>(getI64(d, k));
        for (int j = 0; j < n; ++j) {
            // out(j, k) = j-th MSB-first bit of v (or LSB-first).
            const int bitPos = msbfirst ? (n - 1 - j) : j;
            od[k * n + j] = static_cast<double>((v >> bitPos) & 1);
        }
    }
    return out;
}

// ── bi2de ─────────────────────────────────────────────────────────────
// Legacy synonym: rows of b are digit groups; convert each row to its
// decimal value. Default LSB-first ('right-msb'), default base = 2.
Value bi2de(const Value &b, int base, bool msbfirst,
            std::pmr::memory_resource *mr)
{
    if (b.dims().is3D())
        throw Error("bi2de: input must be 2D",
                    0, 0, "bi2de", "", "numkit:bi2de:Not2D");
    const size_t R = b.dims().rows();
    const size_t C = b.dims().cols();
    Value out = Value::matrix(R, 1, ValueType::DOUBLE, mr);
    if (R == 0) return out;
    double *od = out.doubleDataMut();
    for (size_t r = 0; r < R; ++r) {
        // b is column-major; element (r, c) at idx r + c*R.
        long double v = 0.0L;
        long double pwr = 1.0L;
        for (size_t c = 0; c < C; ++c) {
            const size_t idx = msbfirst ? (C - 1 - c) : c;
            v += static_cast<long double>(getI64(b, r + idx * R)) * pwr;
            pwr *= static_cast<long double>(base);
        }
        od[r] = static_cast<double>(v);
    }
    return out;
}

// ── de2bi ─────────────────────────────────────────────────────────────
// Legacy inverse: each input integer becomes one row of digit values
// (default LSB-first 'right-msb', default base = 2). Width n is the
// minimum number of digits; if not given, computed from the largest input.
Value de2bi(const Value &d, int n_user, int base, bool msbfirst,
            std::pmr::memory_resource *mr)
{
    if (base < 2)
        throw Error("de2bi: base must be >= 2",
                    0, 0, "de2bi", "", "numkit:de2bi:BadBase");
    const size_t R = d.numel();
    // Auto-width: ceil(log_base(max(d)+1)).
    int n = n_user;
    if (n <= 0) {
        uint64_t mx = 0;
        for (size_t i = 0; i < R; ++i) {
            const uint64_t v = static_cast<uint64_t>(getI64(d, i));
            if (v > mx) mx = v;
        }
        n = 0;
        uint64_t t = mx;
        while (t > 0) { ++n; t /= static_cast<uint64_t>(base); }
        if (n == 0) n = 1;
    }
    Value out = Value::matrix(R, static_cast<size_t>(n), ValueType::DOUBLE, mr);
    if (R == 0) return out;
    double *od = out.doubleDataMut();
    for (size_t r = 0; r < R; ++r) {
        uint64_t v = static_cast<uint64_t>(getI64(d, r));
        for (int j = 0; j < n; ++j) {
            const int col = msbfirst ? (n - 1 - j) : j;
            od[r + static_cast<size_t>(col) * R] =
                static_cast<double>(v % static_cast<uint64_t>(base));
            v /= static_cast<uint64_t>(base);
        }
    }
    return out;
}

// ── vec2mat ───────────────────────────────────────────────────────────
// Reshape vector into N-column matrix with row-major fill, padding the
// last row to width N with `padval` (default 0). Returns the matrix and
// the count of pad entries added.
std::pair<Value, int>
vec2mat(const Value &v, int n, double padval, std::pmr::memory_resource *mr)
{
    if (n <= 0)
        throw Error("vec2mat: n must be positive",
                    0, 0, "vec2mat", "", "numkit:vec2mat:BadN");
    const size_t L = v.numel();
    const size_t cols = static_cast<size_t>(n);
    const size_t rows = (L + cols - 1) / cols;
    const size_t pad  = rows * cols - L;
    Value out = Value::matrix(rows, cols, ValueType::DOUBLE, mr);
    if (rows == 0) return {out, 0};
    double *od = out.doubleDataMut();
    // Row-major fill: out(r, c) = v(r * cols + c) when in-range, padval otherwise.
    // out is column-major, so write to (r + c*rows).
    for (size_t r = 0; r < rows; ++r) {
        for (size_t c = 0; c < cols; ++c) {
            const size_t lin = r * cols + c;
            od[r + c * rows] = (lin < L) ? v.elemAsDouble(lin) : padval;
        }
    }
    return {out, static_cast<int>(pad)};
}

} // namespace numkit::comm
