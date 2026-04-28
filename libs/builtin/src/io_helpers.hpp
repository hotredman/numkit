// libs/builtin/src/io_helpers.hpp
//
// Private helpers shared between fileio.cpp (fread/fwrite) and
// scan.cpp (fscanf/sscanf/textscan). Not a public header — lives
// in src/ and is only on the PRIVATE include path for libs/builtin.

#pragma once

#include <numkit/core/allocator.hpp>
#include <numkit/core/types.hpp>
#include <numkit/core/value.hpp>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <limits>
#include <optional>
#include <string>
#include <utility>

namespace numkit::builtin::detail {

// ── SizeSpec: fread / fscanf size-argument parsing ───────────────────
struct SizeSpec
{
    enum class Kind { Flat, Matrix };
    Kind kind;
    size_t limit;
    size_t rows;
    size_t cols;
    bool matrix() const { return kind == Kind::Matrix; }
};

inline SizeSpec parseReadSize(const Value &sz, const char *fn)
{
    if (sz.isScalar()) {
        double s = sz.toScalar();
        if (std::isinf(s))
            return SizeSpec{SizeSpec::Kind::Flat, SIZE_MAX, 0, 0};
        if (s < 0 || !std::isfinite(s))
            throw Error(std::string(fn) + ": size must be Inf or a non-negative integer");
        return SizeSpec{SizeSpec::Kind::Flat, static_cast<size_t>(s), 0, 0};
    }
    if (sz.numel() == 2) {
        double r = sz(0);
        double c = sz(1);
        if (r < 0 || !std::isfinite(r) || std::isinf(r))
            throw Error(std::string(fn) + ": rows in [m n] must be finite non-negative");
        size_t rows = static_cast<size_t>(r);
        size_t cols;
        size_t limit;
        if (std::isinf(c)) {
            cols = SIZE_MAX;
            limit = SIZE_MAX;
        } else {
            if (c < 0 || !std::isfinite(c))
                throw Error(std::string(fn)
                             + ": cols in [m n] must be finite non-negative or Inf");
            cols = static_cast<size_t>(c);
            limit = rows * cols;
        }
        return SizeSpec{SizeSpec::Kind::Matrix, limit, rows, cols};
    }
    throw Error(std::string(fn) + ": size must be scalar, Inf, or a 2-element vector");
}

inline Value shapeFreadOutput(std::vector<double> &&values, SizeSpec sz, Allocator *alloc)
{
    size_t n = values.size();
    if (!sz.matrix()) {
        if (n == 0)
            return Value::matrix(0, 0, ValueType::DOUBLE, alloc);
        Value M = Value::matrix(n, 1, ValueType::DOUBLE, alloc);
        std::memcpy(M.doubleDataMut(), values.data(), n * sizeof(double));
        return M;
    }
    size_t cols_out = (sz.cols == SIZE_MAX)
                          ? (sz.rows == 0 ? 0 : (n + sz.rows - 1) / sz.rows)
                          : sz.cols;
    if (sz.rows == 0 || cols_out == 0)
        return Value::matrix(sz.rows, cols_out, ValueType::DOUBLE, alloc);
    Value M = Value::matrix(sz.rows, cols_out, ValueType::DOUBLE, alloc);
    std::memcpy(M.doubleDataMut(), values.data(), n * sizeof(double));
    return M;
}

// ── Precision + endian parsing (fread / fwrite) ──────────────────────
// kind: 0 = unsigned, 1 = signed, 2 = float. byteSize in bytes.
inline std::optional<std::pair<int, size_t>> parsePrecision(const std::string &raw)
{
    std::string p = raw;
    auto arrow = p.find("=>");
    if (arrow != std::string::npos) p = p.substr(0, arrow);
    while (!p.empty() && std::isspace(static_cast<unsigned char>(p.front()))) p.erase(0, 1);
    while (!p.empty() && std::isspace(static_cast<unsigned char>(p.back()))) p.pop_back();

    if (p == "uint8" || p == "uchar" || p == "char") return std::make_pair(0, size_t{1});
    if (p == "int8"  || p == "schar")                return std::make_pair(1, size_t{1});
    if (p == "uint16")                               return std::make_pair(0, size_t{2});
    if (p == "int16")                                return std::make_pair(1, size_t{2});
    if (p == "uint32")                               return std::make_pair(0, size_t{4});
    if (p == "int32")                                return std::make_pair(1, size_t{4});
    if (p == "uint64")                               return std::make_pair(0, size_t{8});
    if (p == "int64")                                return std::make_pair(1, size_t{8});
    if (p == "single" || p == "float32" || p == "real*4") return std::make_pair(2, size_t{4});
    if (p == "double" || p == "float64" || p == "real*8") return std::make_pair(2, size_t{8});
    return std::nullopt;
}

// Returns true for big-endian, false for little-endian. 'native' → LE
// because every target we support (x86_64, ARM64, WASM) is LE.
inline bool parseEndian(const std::string &raw, const char *fn)
{
    std::string lo;
    lo.reserve(raw.size());
    for (char c : raw) lo += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (lo == "b" || lo == "ieee-be" || lo == "ieee-be.l64") return true;
    if (lo == "l" || lo == "ieee-le" || lo == "ieee-le.l64"
        || lo == "n" || lo == "native")
        return false;
    throw Error(std::string(fn) + ": unsupported machine format '" + raw + "'");
}

inline void byteSwap(char *p, size_t n)
{
    for (size_t i = 0, j = n - 1; i < j; ++i, --j) std::swap(p[i], p[j]);
}

} // namespace numkit::builtin::detail
