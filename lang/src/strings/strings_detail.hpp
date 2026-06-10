// toolboxes/.../strings_detail.hpp — private compute/register substrate (anon-in-
// header, internal linkage per TU) shared by strings.cpp + strings_reg.cpp.
// Phase 2b compute/register split — see project_layering_refactor memory.
#pragma once

#include <numkit/value/value.hpp>
#include <numkit/value/error.hpp>
#include <numkit/value/scratch.hpp>
#include <numkit/value/span.hpp>
#include "reduction_helpers.hpp"  // engine-free numkit::builtin::detail dim-infra (ops re-export)

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory_resource>
#include <numeric>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace numkit::lang {

namespace {
// Format a SCALAR complex value as "re±|im|i" (MATLAB num2str). A common
// precision is used for both parts, derived from max(|re|,|im|) with the same
// magnitude-aware rule as the real default: prec = max(floor(log10 M),0)+5.
// precOverride >= 1 forces that precision (the num2str(z,N) form). An element
// with exactly-zero imaginary part prints as a bare real (num2str(complex(5,0))
// = "5"). The DEFAULT-precision/N forms for complex ARRAYS use MATLAB's
// column-aligned layout and remain a deferred gap (see num2str_reg).
std::string num2strComplexScalar(Complex z, int precOverride)
{
    const double re = z.real(), im = z.imag();
    int prec = precOverride;
    if (prec < 1) {
        const double M = std::max(std::fabs(re), std::fabs(im));
        prec = 5;
        if (M != 0.0) {
            const int e = static_cast<int>(std::floor(std::log10(M)));
            prec = (e > 0 ? e : 0) + 5;
        }
    }
    if (prec > 99) prec = 99;
    auto fmtP = [prec](double v) {
        if (v == 0.0) v = 0.0;   // normalise -0 -> 0
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%.*g", prec, v);
        return std::string(buf);
    };
    if (im == 0.0) return fmtP(re);
    std::string s = fmtP(re);
    s += (im < 0.0 ? '-' : '+');
    s += fmtP(std::fabs(im));
    s += 'i';
    return s;
}

// MATLAB's default num2str column format for a REAL, non-scalar array.
// N >= 1 forces the precision (the num2str(X,N) form); N <= 0 selects the
// default (auto): an all-integer array uses a fixed "%<W>.0f" field where
// W = (max element character count incl sign) + 2; otherwise a magnitude-
// aware "%<W>.<P>g" where P = max(floor(log10(maxabs)),0)+5 and W = P + 7.
// The returned format is applied cyclically per row; the caller routes it
// through the FMT overload, which performs MATLAB's common-blank-column
// stripping and NaN/Inf spelling. Matches MATLAB R2025b across integer,
// fractional, negative, and large/small-magnitude probes.
std::string num2strArrayFormat(const Value &x, int N)
{
    const size_t n = x.numel();
    char fmt[32];
    if (N >= 1) {
        int p = N; if (p > 99) p = 99;
        std::snprintf(fmt, sizeof(fmt), "%%%d.%dg", p + 7, p);
        return std::string(fmt);
    }
    double maxabs = 0.0;
    bool allInt = true;
    int maxChars = 1;
    for (size_t i = 0; i < n; ++i) {
        const double v = x.elemAsDouble(i);
        if (!std::isfinite(v)) { allInt = false; continue; }
        const double a = std::fabs(v);
        if (a > maxabs) maxabs = a;
        if (v != std::floor(v)) allInt = false;
        // Column width is driven by the DIGIT count of |v| (sign excluded):
        // MATLAB's integer field is max-abs-digits + 2, and a minus sign is
        // absorbed into the field rather than widening it — num2str([-1 10
        // -100])='-1   10 -100' (width 5), not width 6. Measuring "%.0f" of v
        // (which includes '-') over-counted by one for negatives.
        char b[64];
        const int len = std::snprintf(b, sizeof(b), "%.0f", a);
        if (len > maxChars) maxChars = len;
    }
    if (allInt) {
        std::snprintf(fmt, sizeof(fmt), "%%%d.0f", maxChars + 2);
        return std::string(fmt);
    }
    int p = 5;
    if (maxabs != 0.0) {
        const int e = static_cast<int>(std::floor(std::log10(maxabs)));
        p = (e > 0 ? e : 0) + 5;
    }
    if (p > 99) p = 99;
    std::snprintf(fmt, sizeof(fmt), "%%%d.%dg", p + 7, p);
    return std::string(fmt);
}
} // namespace
namespace {

// Element-wise string comparison with MATLAB cell-array broadcasting:
//   char/string vs char/string -> scalar logical (whole-string compare)
//   cell vs char/string-scalar  -> logical array shaped like the cell
//   cell vs cell                -> element-wise; sizes must match, or one
//                                  is a scalar (1x1) cell that broadcasts
// `cmp(sa, sb)` is the per-pair predicate (captures n for strncmp).
template <class Pred>
Value strCmpElementwise(const Value &a, const Value &b, Pred cmp,
                        const char *fn, std::pmr::memory_resource *mr)
{
    // An operand is an "array" (compared element-wise) when it is a cell OR
    // a non-scalar string array; a char / string scalar broadcasts against it.
    auto isArr = [](const Value &v) {
        return v.isCell() || (v.isString() && !v.isScalar());
    };
    auto elemAt = [](const Value &v, std::size_t idx) -> std::string {
        return v.isCell() ? v.cellAt(idx).toString() : v.stringElem(idx);
    };
    const bool ac = isArr(a), bc = isArr(b);
    if (!ac && !bc)
        return Value::logicalScalar(cmp(a.toString(), b.toString()), mr);

    const std::size_t na = ac ? a.numel() : 1;
    const std::size_t nb = bc ? b.numel() : 1;
    std::size_t n = 1, rr = 1, cc = 1;
    auto shapeOf = [](const Value &v, std::size_t &r, std::size_t &c) {
        r = static_cast<std::size_t>(v.dims().rows());
        c = static_cast<std::size_t>(v.dims().cols());
    };
    if (ac && bc) {
        if (na == nb)      { n = na; shapeOf(a, rr, cc); }
        else if (na == 1)  { n = nb; shapeOf(b, rr, cc); }
        else if (nb == 1)  { n = na; shapeOf(a, rr, cc); }
        else
            throw Error(std::string(fn) + ": cell array sizes must match",
                        0, 0, fn, "", std::string("numkit:") + fn + ":cellSize");
    } else if (ac) { n = na; shapeOf(a, rr, cc); }
    else           { n = nb; shapeOf(b, rr, cc); }

    auto out = Value::matrix(rr, cc, ValueType::LOGICAL, mr);
    auto *od = out.logicalDataMut();
    const std::string scalA = ac ? std::string() : a.toString();
    const std::string scalB = bc ? std::string() : b.toString();
    for (std::size_t i = 0; i < n; ++i) {
        const std::string sa = ac ? elemAt(a, na == 1 ? 0 : i) : scalA;
        const std::string sb = bc ? elemAt(b, nb == 1 ? 0 : i) : scalB;
        od[i] = cmp(sa, sb) ? 1 : 0;
    }
    return out;
}

// strncmp/strncmpi predicate: if BOTH strings are at least n chars, compare
// the first n; otherwise require full equality (MATLAB: strncmp('ab','ab',5)
// is true, strncmp('ab','abc',5) is false).
inline bool strnEq(const std::string &sa, const std::string &sb, size_t n)
{
    if (sa.size() >= n && sb.size() >= n)
        return sa.compare(0, n, sb, 0, n) == 0;
    return sa == sb;
}

} // namespace
namespace {

// Length of the LONGEST delimiter (literal) in `delims` matching `s` at
// position `i`, or 0 if none match. Longest-match so a delimiter ", " wins
// over "," when both are listed.
size_t matchDelimAt(const std::string &s, size_t i,
                    const std::pmr::vector<std::string> &delims)
{
    size_t best = 0;
    for (const auto &d : delims)
        if (!d.empty() && d.size() > best && i + d.size() <= s.size() &&
            s.compare(i, d.size(), d) == 0)
            best = d.size();
    return best;
}

// Core split used by every strsplit entry point. Matches MATLAB R2025b:
//   - any of `delims` (literal, longest-match) is a split point;
//   - CollapseDelimiters=true (default) merges only CONSECUTIVE delimiters,
//     so leading/trailing empty tokens are still produced
//     (',a,b,' -> {'','a','b',''}); collapse=false splits at every
//     occurrence ('a,,b' -> {'a','','b'});
//   - the result always has at least one element ('' -> {''}).
// Splits `s` on `delims`. When `matchesOut` is non-null it receives the
// matched delimiter text at each split point (the whole collapsed run when
// collapse is on), supporting MATLAB's [tokens, matches] = strsplit(...).
Value strsplitImpl(const std::string &s,
                   const std::pmr::vector<std::string> &delims, bool collapse,
                   std::pmr::memory_resource *mr,
                   ScratchVec<std::string> *matchesOut = nullptr)
{
    ScratchArena scratch(mr);
    ScratchVec<std::string> parts(&scratch);
    std::string current;
    const size_t n = s.size();
    size_t i = 0;
    while (i < n) {
        size_t mlen = matchDelimAt(s, i, delims);
        if (mlen > 0) {
            parts.push_back(current);
            current.clear();
            const size_t matchStart = i;
            i += mlen;
            if (collapse) {
                size_t m2;
                while (i < n && (m2 = matchDelimAt(s, i, delims)) > 0)
                    i += m2;
            }
            if (matchesOut)
                matchesOut->push_back(s.substr(matchStart, i - matchStart));
        } else {
            current.push_back(s[i++]);
        }
    }
    parts.push_back(current);
    auto c = Value::cell(1, parts.size());
    for (size_t k = 0; k < parts.size(); ++k)
        c.cellAt(k) = Value::fromString(parts[k], mr);
    return c;
}

// Append the MATLAB default whitespace delimiter set to `delims`.
void appendDefaultWhitespace(ScratchVec<std::string> &delims)
{
    const char ws[] = {' ', '\t', '\n', '\r', '\f', '\v'};
    for (char w : ws)
        delims.push_back(std::string(1, w));
}

// Build the delimiter list from a string OR a cell array of strings.
void appendDelims(const Value &delim, ScratchVec<std::string> &delims)
{
    if (delim.isCell())
        for (size_t i = 0; i < delim.numel(); ++i)
            delims.push_back(delim.cellAt(i).toString());
    else
        delims.push_back(delim.toString());
}

} // namespace
namespace {
// All 1-based indices of `pp` in `ss` (overlapping, like MATLAB) as a 1×k
// DOUBLE row vector; [] (0×0) if no match or `pp` is empty.
Value strfindOne(const std::string &ss, const std::string &pp,
                 std::pmr::memory_resource *mr)
{
    if (pp.empty())
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);  // strfind(s,'') -> []
    ScratchArena scratch(mr);
    ScratchVec<size_t> hits(&scratch);
    size_t pos = 0;
    while ((pos = ss.find(pp, pos)) != std::string::npos) {
        hits.push_back(pos + 1);  // 1-based
        pos += 1;                 // overlapping matches like MATLAB
    }
    if (hits.empty())
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);
    auto r = Value::matrix(1, hits.size(), ValueType::DOUBLE, mr);
    for (size_t i = 0; i < hits.size(); ++i)
        r.doubleDataMut()[i] = static_cast<double>(hits[i]);
    return r;
}
} // anonymous
namespace {

// Collect the pattern strings from `pat`: a char/string scalar yields one
// pattern; a cell array of char vectors or a multi-element string array yields
// one per element (both store their elements as Values in cellDataVec()).
// Shared by contains/startsWith/endsWith (match-any) and count/erase
// (apply each listed pattern).
void collectMatchPatterns(const Value &pat, ScratchVec<std::string> &out)
{
    if (pat.isCell() || (pat.isString() && pat.numel() != 1)) {
        const auto &vec = pat.cellDataVec();
        for (const auto &e : vec) out.push_back(e.toString());
    } else {
        out.push_back(pat.toString());
    }
}

// ASCII lower-case in place — used by the 'IgnoreCase' option of
// contains/startsWith/endsWith.
inline void asciiLowerInPlace(std::string &x)
{
    for (char &c : x)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

} // namespace
namespace {
inline std::string readSide(const Value &side, const char *def)
{
    if (side.isEmpty()) return def;
    if (!side.isChar() && !side.isString())
        throw std::runtime_error("string-side argument must be a string");
    auto s = side.toString();
    for (auto &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
} // anon
namespace {
// Build a logical array shaped like the input char/string by running
// `predFn(c)` over each character.
template <typename PredFn>
Value applyCharPred(const Value &s, PredFn pred, std::pmr::memory_resource *mr)
{
    if (s.isChar()) {
        const std::string str = s.toString();
        const size_t n = str.size();
        // Mirror char's shape (1×N for typical char rows).
        const auto &d = s.dims();
        auto r = (d.is3D()
                     ? Value::matrix3d(d.rows(), d.cols(), d.pages(),
                                       ValueType::LOGICAL, mr)
                     : Value::matrix(d.rows(), d.cols(),
                                     ValueType::LOGICAL, mr));
        for (size_t i = 0; i < n && i < r.numel(); ++i)
            r.logicalDataMut()[i] = pred(static_cast<unsigned char>(str[i])) ? 1 : 0;
        return r;
    }
    if (s.isString()) {
        // For string array, MATLAB returns a cell array of logical
        // arrays (one per element). For now support scalar strings.
        if (s.numel() == 1) {
            const std::string str = s.toString();
            auto r = Value::matrix(1, str.size(), ValueType::LOGICAL, mr);
            for (size_t i = 0; i < str.size(); ++i)
                r.logicalDataMut()[i] = pred(static_cast<unsigned char>(str[i])) ? 1 : 0;
            return r;
        }
        throw Error("char-predicate: string-array form not supported",
                     0, 0, "isstrprop", "", "numkit:isstrprop:stringArray");
    }
    throw Error("char-predicate: input must be char or string",
                 0, 0, "isstrprop", "", "numkit:isstrprop:type");
}
} // anon
namespace {
struct PosRange { size_t begin; size_t end; bool found; };

// Resolve `p` to a [begin, end) range in `s`. For a numeric position
// p, [p-1, p) is returned (so callers can treat it as a "single
// character" anchor). For a string pattern, [pos, pos+len).
PosRange resolvePos(const std::string &s, const Value &p)
{
    if (p.isChar() || p.isString()) {
        const std::string pat = p.toString();
        if (pat.empty()) return {0, 0, false};
        const size_t pos = s.find(pat);
        if (pos == std::string::npos) return {0, 0, false};
        return {pos, pos + pat.size(), true};
    }
    // Numeric scalar (1-based).
    const auto p1 = static_cast<long long>(p.toScalar());
    if (p1 < 1 || static_cast<size_t>(p1) > s.size())
        return {0, 0, false};
    return {static_cast<size_t>(p1 - 1), static_cast<size_t>(p1), true};
}

inline Value strLikeOf(const Value &s, const std::string &out, std::pmr::memory_resource *mr)
{
    if (s.isString()) return Value::stringScalar(out, mr);
    return Value::fromString(out, mr);
}
} // anon
namespace {

// Find all non-overlapping (start, end) delimiter pairs in `ss`. Each
// match's `begin` is the index of the opening delimiter, `end` is one
// past the closing delimiter, and `between` brackets just the inner
// text (between the two delimiters). Numeric (position-arg) inputs
// return at most one pair. See BUGS.md #27.
struct BetweenMatch {
    size_t openBegin;   // index of first char of opening delimiter
    size_t openEnd;     // index just past opening delimiter (start of inner)
    size_t closeBegin;  // index of first char of closing delimiter (end of inner)
    size_t closeEnd;    // index just past closing delimiter
};

std::vector<BetweenMatch>
findAllBetweenPairs(const std::string &ss, const Value &start, const Value &end)
{
    std::vector<BetweenMatch> matches;
    const bool startIsPattern = (start.isChar() || start.isString());
    const bool endIsPattern   = (end.isChar()   || end.isString());

    if (!startIsPattern || !endIsPattern) {
        // Numeric-anchor form: at most one match.
        const auto rs = resolvePos(ss, start);
        if (!rs.found) return matches;
        const std::string tail = ss.substr(rs.end);
        const auto re = resolvePos(tail, end);
        if (!re.found) return matches;
        matches.push_back({rs.begin, rs.end, rs.end + re.begin, rs.end + re.end});
        return matches;
    }

    const std::string sp = start.toString();
    const std::string ep = end.toString();
    if (sp.empty() || ep.empty()) return matches;

    size_t cursor = 0;
    while (cursor < ss.size()) {
        const size_t sPos = ss.find(sp, cursor);
        if (sPos == std::string::npos) break;
        const size_t innerBeg = sPos + sp.size();
        const size_t ePos = ss.find(ep, innerBeg);
        if (ePos == std::string::npos) break;
        matches.push_back({sPos, innerBeg, ePos, ePos + ep.size()});
        // Advance past the closing delimiter so non-overlapping pairs
        // are matched — `<<a>><<b>>` yields 2 matches, not 1.
        cursor = ePos + ep.size();
    }
    return matches;
}

} // namespace
namespace {
std::string toBaseString(uint64_t v, int base, int minWidth)
{
    if (v == 0) {
        std::string s(std::max(1, minWidth), '0');
        return s;
    }
    std::string out;
    while (v > 0) {
        const int d = static_cast<int>(v % static_cast<uint64_t>(base));
        out.push_back(d < 10 ? char('0' + d) : char('A' + d - 10));
        v /= static_cast<uint64_t>(base);
    }
    while (static_cast<int>(out.size()) < minWidth) out.push_back('0');
    std::reverse(out.begin(), out.end());
    return out;
}

uint64_t parseBase(const std::string &s, int base)
{
    uint64_t v = 0;
    for (char c : s) {
        if (std::isspace(static_cast<unsigned char>(c))) continue;
        int d;
        if (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = 10 + (c - 'a');
        else if (c >= 'A' && c <= 'F') d = 10 + (c - 'A');
        else throw Error(std::string("invalid digit '") + c + "' for base "
                          + std::to_string(base),
                          0, 0, "base", "", "numkit:base:badDigit");
        if (d >= base)
            throw Error(std::string("digit '") + c + "' out of range for base "
                          + std::to_string(base),
                          0, 0, "base", "", "numkit:base:badDigit");
        v = v * static_cast<uint64_t>(base) + static_cast<uint64_t>(d);
    }
    return v;
}

// Convert an N-element double vector to a 2-D char matrix where each
// row holds the base-`base` representation, padded to the maximum
// observed width (and at least minWidth).
Value vecToBaseMatrix(const Value &d, int base, int minWidth, std::pmr::memory_resource *mr)
{
    const size_t n = d.numel();
    if (n == 0) return Value::fromString("", mr);
    if (n == 1) {
        const double v = d.toScalar();
        if (v < 0) throw Error("dec2*: value must be non-negative",
                                0, 0, "dec2", "", "numkit:dec2:negative");
        return Value::fromString(
            toBaseString(static_cast<uint64_t>(v), base, minWidth), mr);
    }
    // Compute max width.
    ScratchArena scratch(mr);
    auto rows = ScratchVec<std::string>(&scratch);
    rows.reserve(n);
    int maxW = minWidth;
    for (size_t i = 0; i < n; ++i) {
        const double v = d.elemAsDouble(i);
        if (v < 0) throw Error("dec2*: value must be non-negative",
                                0, 0, "dec2", "", "numkit:dec2:negative");
        rows.emplace_back(toBaseString(static_cast<uint64_t>(v), base, 0));
        maxW = std::max<int>(maxW, static_cast<int>(rows.back().size()));
    }
    // Pad each row to maxW with leading zeros.
    for (auto &r : rows)
        while (static_cast<int>(r.size()) < maxW)
            r.insert(r.begin(), '0');
    // Build a CHAR matrix n × maxW, column-major.
    auto m = Value::matrix(n, maxW, ValueType::CHAR, mr);
    char *dst = static_cast<char *>(m.rawDataMut());
    for (size_t r = 0; r < n; ++r)
        for (int c = 0; c < maxW; ++c)
            dst[c * n + r] = rows[r][c];
    return m;
}
} // anon
namespace {
// Interpret a hex string as the raw IEEE-754 bit pattern of a double.
// Whitespace is skipped (a char matrix pads short rows with spaces); the
// digits are right-padded with '0' to 16 (MATLAB: hex2num('4') == 2).
double hexBitsToDouble(const std::string &raw)
{
    std::string h;
    h.reserve(16);
    for (char c : raw) {
        if (std::isspace(static_cast<unsigned char>(c))) continue;
        h.push_back(c);
    }
    if (h.size() > 16) h.resize(16);
    while (h.size() < 16) h.push_back('0');  // right-pad with zeros
    uint64_t bits = 0;
    for (char c : h) {
        int d;
        if      (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'f') d = 10 + (c - 'a');
        else if (c >= 'A' && c <= 'F') d = 10 + (c - 'A');
        else throw Error(std::string("hex2num: invalid hex digit '") + c + "'",
                          0, 0, "hex2num", "", "numkit:hex2num:badDigit");
        bits = (bits << 4) | static_cast<uint64_t>(d);
    }
    double out;
    std::memcpy(&out, &bits, sizeof(out));
    return out;
}
} // anon
namespace {
// Parse a base-`base` (2..36) digit string: 0-9 then A-Z / a-z. Whitespace
// is skipped (MATLAB pads short rows of a char matrix with spaces).
uint64_t parseBaseN(const std::string &s, int base)
{
    uint64_t v = 0;
    for (char c : s) {
        if (std::isspace(static_cast<unsigned char>(c))) continue;
        int d;
        if      (c >= '0' && c <= '9') d = c - '0';
        else if (c >= 'a' && c <= 'z') d = 10 + (c - 'a');
        else if (c >= 'A' && c <= 'Z') d = 10 + (c - 'A');
        else throw Error(std::string("base2dec: invalid digit '") + c + "'",
                          0, 0, "base2dec", "", "numkit:base2dec:badDigit");
        if (d >= base)
            throw Error(std::string("base2dec: digit '") + c +
                            "' out of range for base " + std::to_string(base),
                          0, 0, "base2dec", "", "numkit:base2dec:badDigit");
        v = v * static_cast<uint64_t>(base) + static_cast<uint64_t>(d);
    }
    return v;
}
} // anon
namespace {

// MATLAB's rat() uses the **regularized** continued-fraction expansion:
// at each step it picks `a = round(r)` (nearest integer, half-away-from-
// zero), not floor(r). This produces signed coefficients (e.g. 0.5 →
// `[1, -2]`, recovering 1 + 1/-2 = 0.5) and matches MATLAB's display
// `'1 + 1/(-2)'` exactly. The standard CF recurrence h_n = a_n·h_{n-1}
// + h_{n-2}, k_n = a_n·k_{n-1} + k_{n-2} works unchanged for signed a_n.
//
// Returns the sequence of CF coefficients (a_0, a_1, ...) plus the
// final converged p/q (denominator normalised positive). Stops when
// |x - p/q| ≤ tol, with tol defaulting (caller-side) to 1e-6·max(1,|x|).
struct RatExpansion {
    long long p = 0;
    long long q = 0;
    // Up to 64 coefficients — same safety bound as before. Inline
    // storage avoids any allocation for the typical 2-7 coefficient case.
    std::array<long long, 64> coeffs;
    int n_coeffs = 0;
};

RatExpansion ratExpansion(double x, double tol)
{
    RatExpansion r;
    if (!std::isfinite(x)) return r;

    double rem = x;
    // Track the last two convergents.
    long long prev_p = 0, curr_p = 1;
    long long prev_q = 1, curr_q = 0;

    for (int i = 0; i < 64; ++i) {
        const long long a = static_cast<long long>(std::round(rem));
        const long long new_p = a * curr_p + prev_p;
        const long long new_q = a * curr_q + prev_q;
        r.coeffs[r.n_coeffs++] = a;
        prev_p = curr_p; curr_p = new_p;
        prev_q = curr_q; curr_q = new_q;

        if (curr_q != 0) {
            const double approx = static_cast<double>(curr_p)
                                / static_cast<double>(curr_q);
            if (std::fabs(approx - x) <= tol) break;
        }
        const double frac = rem - static_cast<double>(a);
        if (frac == 0.0) break;
        rem = 1.0 / frac;
        if (std::fabs(rem) > 1e15) break;
    }

    r.p = curr_p;
    r.q = curr_q;
    if (r.q < 0) { r.p = -r.p; r.q = -r.q; }
    return r;
}

// Build MATLAB's nested CF-string from the coefficient sequence:
//   1 coeff   →  "a0"
//   2 coeffs  →  "a0 + 1/(a1)"
//   3 coeffs  →  "a0 + 1/(a1 + 1/(a2))"
//   n coeffs  →  "a0 + 1/(a1 + 1/( ... + 1/(an-1)))"
std::string buildCFString(const RatExpansion &r)
{
    if (r.n_coeffs == 0) return "0";
    std::string s = std::to_string(r.coeffs[r.n_coeffs - 1]);
    for (int i = r.n_coeffs - 2; i >= 0; --i)
        s = std::to_string(r.coeffs[i]) + " + 1/(" + s + ")";
    return s;
}

// Default tol per MATLAB docs: `1e-6 * norm(X(:),1)`. For a scalar
// input that's `1e-6 * |x|`, with a small floor at 1e-6 so x=0 still
// terminates (the loop hits frac == 0 at i=0 anyway, but keep safe).
inline double defaultRatTol(double x)
{
    return 1e-6 * std::max(1.0, std::fabs(x));
}

} // anonymous
namespace {
enum class StrPred { Contains, StartsWith, EndsWith };

// Test one source string against the collected patterns (any-match) under the
// chosen predicate. ignoreCase ASCII-lowercases both sides.
bool strMatchAny(std::string src, const ScratchVec<std::string> &pats,
                 bool ignoreCase, StrPred pred)
{
    if (ignoreCase) asciiLowerInPlace(src);
    for (std::string pp : pats) {
        if (ignoreCase) asciiLowerInPlace(pp);
        bool hit = false;
        switch (pred) {
        case StrPred::Contains:
            hit = src.find(pp) != std::string::npos;
            break;
        case StrPred::StartsWith:
            hit = src.size() >= pp.size() && src.compare(0, pp.size(), pp) == 0;
            break;
        case StrPred::EndsWith:
            hit = src.size() >= pp.size()
                  && src.compare(src.size() - pp.size(), pp.size(), pp) == 0;
            break;
        }
        if (hit) return true;
    }
    return false;
}

// Shared driver for contains/startsWith/endsWith: a cell array or a non-scalar
// string-array SOURCE maps element-wise to a LOGICAL array of the same shape
// (MATLAB); a scalar char/string source returns a logical scalar. The pattern
// argument may itself be a scalar or a cell/string array (any-match).
Value strPredicate(const Value &s, const Value &pat, bool ignoreCase,
                   StrPred pred, std::pmr::memory_resource *mr)
{
    ScratchArena scratch(mr);
    ScratchVec<std::string> pats(&scratch);
    collectMatchPatterns(pat, pats);

    if (s.isCell() || (s.isString() && s.numel() != 1)) {
        const size_t r = static_cast<size_t>(s.dims().rows());
        const size_t c = static_cast<size_t>(s.dims().cols());
        Value out = Value::matrix(r, c, ValueType::LOGICAL, mr);
        uint8_t *od = out.logicalDataMut();
        const size_t n = s.numel();
        for (size_t i = 0; i < n; ++i) {
            const std::string el =
                s.isCell() ? s.cellAt(i).toString() : s.stringElem(i);
            od[i] = strMatchAny(el, pats, ignoreCase, pred) ? 1 : 0;
        }
        return out;
    }
    return Value::logicalScalar(
        strMatchAny(s.toString(), pats, ignoreCase, pred), mr);
}
} // namespace
namespace {

// Read element i of x as a Value scalar to feed sprintf.
Value elemScalar(const Value &x, size_t i, std::pmr::memory_resource *mr)
{
    if (x.isNumeric()) {
        return Value::scalar(x.elemAsDouble(i), mr);
    }
    if (x.isLogical()) {
        return Value::logicalScalar(x.elemAsDouble(i) != 0, mr);
    }
    if (x.isString())  return Value::stringScalar(x.stringElem(i), mr);
    if (x.isCell())    return x.cellAt(i);  // cell-of-strings allowed
    return Value::scalar(0.0, mr);
}

// Get a char input string from char or string-scalar.
const std::string strInput(const Value &v)
{
    if (v.isChar() || (v.isString() && v.isScalar())) return v.toString();
    if (v.isString()) return v.stringElem(0);  // first elem of array
    return v.toString();  // last-resort coercion
}

// Find every non-overlapping occurrence of `needle` in `hay` and append
// each matched substring to `out`.
void findAllLiteral(const std::string &hay, const std::string &needle,
                    std::vector<std::string> &out)
{
    if (needle.empty()) return;
    size_t pos = 0;
    while (true) {
        size_t f = hay.find(needle, pos);
        if (f == std::string::npos) break;
        out.push_back(needle);
        pos = f + needle.size();
    }
}

// Split `s` by `delim` keeping empty tokens (MATLAB `split` semantics).
void splitKeepEmpty(const std::string &s, const std::string &delim,
                    std::vector<std::string> &out)
{
    if (delim.empty()) { out.push_back(s); return; }
    size_t pos = 0;
    while (true) {
        size_t f = s.find(delim, pos);
        if (f == std::string::npos) {
            out.push_back(s.substr(pos));
            return;
        }
        out.push_back(s.substr(pos, f - pos));
        pos = f + delim.size();
    }
}

} // namespace

} // namespace numkit::lang
