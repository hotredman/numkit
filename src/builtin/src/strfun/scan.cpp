// toolboxes/builtin/src/datatypes/strings/scan.cpp
//
// Scan-family builtins (fscanf / sscanf / textscan). Shares SizeSpec /
// parseReadSize / shapeFreadOutput with fileio.cpp via io_helpers.hpp.

#include <numkit/builtin/strfun.hpp>
#include "scan_core.hpp"

#include <numkit/value/value.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/error.hpp>

#include <numkit/ops/io_helpers.hpp>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <memory_resource>
#include <string>
#include <utility>

namespace numkit::builtin {
using detail::scanfEmit;   // shared core; definition below + runtime fscanf shim
using detail::ScanfOut;

// ════════════════════════════════════════════════════════════════════════
// scanf-cycle primitives (shared between fscanf and sscanf)
// ════════════════════════════════════════════════════════════════════════

namespace {


// Does the format contain any non-text conversion? Controls output
// type. Text-only → char array; anything else → double column.
bool formatHasNumeric(const std::string &fmt)
{
    for (size_t i = 0; i < fmt.size(); ++i) {
        if (fmt[i] != '%') continue;
        ++i;
        if (i < fmt.size() && fmt[i] == '*') ++i;
        while (i < fmt.size() && std::isdigit(static_cast<unsigned char>(fmt[i]))) ++i;
        if (i >= fmt.size()) break;
        char spec = fmt[i];
        if (spec == 'd' || spec == 'i' || spec == 'u' ||
            spec == 'f' || spec == 'e' || spec == 'g' ||
            spec == 'E' || spec == 'G' ||
            spec == 'x' || spec == 'X' || spec == 'o')
            return true;
    }
    return false;
}

ScanfOut scanfCycle(const std::string &input, const std::string &fmt, size_t limit,
                     ScratchVec<double> &out)
{
    size_t inPos = 0;
    size_t count = 0;

    while (count < limit && inPos < input.size()) {
        size_t beforeIn = inPos;
        size_t beforeCount = count;
        size_t fmtPos = 0;
        bool ok = true;

        while (fmtPos < fmt.size() && ok) {
            char fc = fmt[fmtPos];
            if (std::isspace(static_cast<unsigned char>(fc))) {
                while (inPos < input.size() &&
                       std::isspace(static_cast<unsigned char>(input[inPos])))
                    ++inPos;
                ++fmtPos;
                continue;
            }
            if (fc != '%') {
                if (inPos >= input.size() || input[inPos] != fc) { ok = false; break; }
                ++inPos;
                ++fmtPos;
                continue;
            }
            ++fmtPos;
            if (fmtPos >= fmt.size()) { ok = false; break; }

            bool suppress = false;
            if (fmt[fmtPos] == '*') { suppress = true; ++fmtPos; }
            int width = -1;
            while (fmtPos < fmt.size() &&
                   std::isdigit(static_cast<unsigned char>(fmt[fmtPos]))) {
                if (width < 0) width = 0;
                width = width * 10 + (fmt[fmtPos] - '0');
                ++fmtPos;
            }
            if (fmtPos >= fmt.size()) { ok = false; break; }

            // %[set] is a char-class conversion — peek instead of
            // consuming so we can parse the full [abc^-] body.
            if (fmt[fmtPos] == '[') {
                ++fmtPos; // past '['
                bool negate = false;
                if (fmtPos < fmt.size() && fmt[fmtPos] == '^') {
                    negate = true;
                    ++fmtPos;
                }
                std::array<bool, 256> member{};
                bool first = true;
                while (fmtPos < fmt.size() && (first || fmt[fmtPos] != ']')) {
                    first = false;
                    char c = fmt[fmtPos];
                    if (fmtPos + 2 < fmt.size() && fmt[fmtPos + 1] == '-'
                        && fmt[fmtPos + 2] != ']') {
                        unsigned lo = static_cast<unsigned char>(c);
                        unsigned hi = static_cast<unsigned char>(fmt[fmtPos + 2]);
                        if (lo > hi) std::swap(lo, hi);
                        for (unsigned ch = lo; ch <= hi; ++ch)
                            member[ch] = true;
                        fmtPos += 3;
                    } else {
                        member[static_cast<unsigned char>(c)] = true;
                        ++fmtPos;
                    }
                }
                if (fmtPos >= fmt.size()) { ok = false; break; }
                ++fmtPos; // past closing ']'

                // %[set] does NOT skip leading whitespace (like %c).
                if (inPos >= input.size()) { ok = false; break; }
                size_t tokenStart = inPos;
                size_t maxEnd = (width < 0)
                                    ? input.size()
                                    : std::min(inPos + static_cast<size_t>(width),
                                               input.size());
                while (inPos < maxEnd) {
                    bool m = member[static_cast<unsigned char>(input[inPos])];
                    if (negate ? m : !m) break;
                    ++inPos;
                }
                if (inPos == tokenStart) { ok = false; break; }
                if (!suppress) {
                    for (size_t p = tokenStart; p < inPos && count < limit; ++p) {
                        out.push_back(static_cast<double>(
                            static_cast<unsigned char>(input[p])));
                        ++count;
                    }
                }
                continue;
            }

            char spec = fmt[fmtPos++];

            // Numeric + %s skip leading whitespace. %c does NOT —
            // it matches literal characters including whitespace.
            if (spec != 'c') {
                while (inPos < input.size() &&
                       std::isspace(static_cast<unsigned char>(input[inPos])))
                    ++inPos;
            }
            if (inPos >= input.size()) { ok = false; break; }

            if (spec == 's') {
                size_t tokenStart = inPos;
                size_t maxEnd = (width < 0)
                                    ? input.size()
                                    : std::min(inPos + static_cast<size_t>(width),
                                               input.size());
                while (inPos < maxEnd &&
                       !std::isspace(static_cast<unsigned char>(input[inPos])))
                    ++inPos;
                if (inPos == tokenStart) { ok = false; break; }
                if (!suppress) {
                    for (size_t p = tokenStart; p < inPos && count < limit; ++p) {
                        out.push_back(static_cast<double>(
                            static_cast<unsigned char>(input[p])));
                        ++count;
                    }
                }
                continue;
            }

            if (spec == 'c') {
                size_t n = (width < 0) ? 1u : static_cast<size_t>(width);
                if (inPos + n > input.size()) { ok = false; break; }
                if (!suppress) {
                    for (size_t p = 0; p < n && count < limit; ++p) {
                        out.push_back(static_cast<double>(
                            static_cast<unsigned char>(input[inPos + p])));
                        ++count;
                    }
                }
                inPos += n;
                continue;
            }

            const char *start = input.c_str() + inPos;
            char *endp = nullptr;
            double v = 0.0;

            switch (spec) {
            case 'd': case 'i': {
                long long iv = std::strtoll(start, &endp, 10);
                if (endp == start) { ok = false; break; }
                v = static_cast<double>(iv);
                break;
            }
            case 'u': {
                unsigned long long uv = std::strtoull(start, &endp, 10);
                if (endp == start) { ok = false; break; }
                v = static_cast<double>(uv);
                break;
            }
            case 'f': case 'e': case 'g': case 'E': case 'G': {
                v = std::strtod(start, &endp);
                if (endp == start) { ok = false; break; }
                break;
            }
            case 'x': case 'X': {
                unsigned long long uv = std::strtoull(start, &endp, 16);
                if (endp == start) { ok = false; break; }
                v = static_cast<double>(uv);
                break;
            }
            case 'o': {
                unsigned long long uv = std::strtoull(start, &endp, 8);
                if (endp == start) { ok = false; break; }
                v = static_cast<double>(uv);
                break;
            }
            default:
                throw Error(std::string("scanf: unsupported conversion '%")
                                + spec + "'");
            }
            if (!ok) break;

            inPos += static_cast<size_t>(endp - start);
            if (!suppress) {
                out.push_back(v);
                if (++count >= limit) break;
            }
        }

        // No progress this cycle → bail. Roll back any partially-
        // consumed input so ftell/fscanf can retry from a clean
        // boundary if the caller wants to.
        if (count == beforeCount) {
            inPos = beforeIn;
            break;
        }
    }
    return ScanfOut{count, inPos};
}

Value makeColumn(const double *vals, size_t n, std::pmr::memory_resource *mr)
{
    if (n == 0)
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    auto M = Value::matrix(n, 1, ValueType::DOUBLE, mr);
    std::memcpy(M.doubleDataMut(), vals, n * sizeof(double));
    return M;
}

Value makeCharRow(const double *vals, size_t n, std::pmr::memory_resource *mr)
{
    std::string s;
    s.reserve(n);
    for (size_t i = 0; i < n; ++i)
        s.push_back(static_cast<char>(static_cast<int>(vals[i])));
    return Value::fromString(s, mr);
}

// Column-major char matrix from the flat `vals` buffer. Unfilled
// cells stay zero (MATLAB's documented fill for partial char reads).
Value makeCharMatrix(const double *vals, size_t n,
                     numkit::ops::SizeSpec sz, std::pmr::memory_resource *mr)
{
    size_t cols_out = (sz.cols == SIZE_MAX)
                         ? (sz.rows == 0 ? 0 : (n + sz.rows - 1) / sz.rows)
                         : sz.cols;
    if (sz.rows == 0 || cols_out == 0)
        return Value::matrix(sz.rows, cols_out, ValueType::CHAR, mr);
    Value M = Value::matrix(sz.rows, cols_out, ValueType::CHAR, mr);
    char *data = M.charDataMut();
    for (size_t i = 0; i < n; ++i)
        data[i] = static_cast<char>(static_cast<int>(vals[i]));
    return M;
}

// Chooses the output shape per the MATLAB contract: char array when
// the format has only %s/%c conversions, column-of-doubles otherwise.
// Takes a pre-computed `hasNumericConv` flag so the caller doesn't
// need to re-walk the format string.
Value shapeScanfOutput(const double *vals, size_t n,
                       bool hasNumericConv, std::pmr::memory_resource *mr)
{
    if (hasNumericConv)
        return makeColumn(vals, n, mr);
    return makeCharRow(vals, n, mr);
}

} // namespace (anonymous scan helpers)

// ── scanfEmit: shared fscanf/sscanf parse+shape core (exposed in C6c-2b) ──
// Pure (no Engine), so it stays in lang. The engine-coupled fscanf shim
// (relocated to the runtime layer) reads the file buffer then delegates here;
// the pure sscanf below calls it directly. It reaches the file-local anon
// helpers above — they are injected into numkit::lang, hence visible from
// numkit::builtin::detail by ordinary unqualified lookup.
namespace detail {

void scanfEmit(const std::string &input, const std::string &fmt,
               const numkit::ops::SizeSpec &sz,
               size_t nargout, Span<Value> outs, std::pmr::memory_resource *mr, ScanfOut &r)
{
    ScratchArena scratch(mr);
    ScratchVec<double> values(&scratch);
    r = scanfCycle(input, fmt, sz.limit, values);
    const bool hasNum = formatHasNumeric(fmt);
    // Matrix-shape dispatch: numeric format → double matrix,
    // pure-text format → char matrix. Flat size keeps the
    // per-format column-or-row shape from shapeScanfOutput.
    if (sz.matrix() && hasNum)
        outs[0] = numkit::ops::shapeFreadOutput(values.data(), values.size(), sz, mr);
    else if (sz.matrix())
        outs[0] = makeCharMatrix(values.data(), values.size(), sz, mr);
    else
        outs[0] = shapeScanfOutput(values.data(), values.size(), hasNum, mr);
    if (nargout > 1)
        outs[1] = Value::scalar(static_cast<double>(r.count), mr);
}

} // namespace detail

// ════════════════════════════════════════════════════════════════════════
// fscanf / sscanf
//
// Formatted reading. The format string is applied CYCLICALLY to the
// input until either (a) the input runs out / a match fails or (b)
// `size` elements have been produced. The format is a scanf-style
// subset:
//
//   %d  %i  %u        decimal integer
//   %f  %e  %g        float / scientific / general
//   %x  %X            hex
//   %o                octal
//   %s                whitespace-delimited token (skips leading ws)
//   %c                exactly N characters (default 1, no ws skip)
//   %*…               suppress (read but don't emit)
//   width digits      max chars for %s/%c; ignored for numeric
//
// Output type follows MATLAB's rule: if the format contains ANY
// numeric conversion, the result is a column vector of doubles
// (where %s/%c characters become ASCII codes). If the format has
// only text conversions, the result is a char array with the
// characters concatenated in order.
// ════════════════════════════════════════════════════════════════════════


void sscanf(Span<const Value> args, size_t nargout, Span<Value> outs, std::pmr::memory_resource *mr)
{
    if (args.size() < 2 || !args[0].isChar() || !args[1].isChar())
        throw Error("sscanf: requires (str, format [, size])");

    numkit::ops::SizeSpec sz{numkit::ops::SizeSpec::Kind::Flat, SIZE_MAX, 0, 0};
    if (args.size() >= 3)
        sz = numkit::ops::parseReadSize(args[2], "sscanf");

    std::string fmt = args[1].toString();
    ScanfOut r{0, 0};
    scanfEmit(args[0].toString(), fmt, sz, nargout, outs, mr, r);

    if (nargout > 2)
        outs[2] = Value::fromString("", mr); // errmsg — always empty for now
    if (nargout > 3)
        outs[3] = Value::scalar(static_cast<double>(r.bytesConsumed + 1), mr);
}


} // namespace numkit::builtin
