// libs/builtin/src/datatypes/strings/strings.cpp
//
// Non-regex string builtins: num2str / str2num / str2double / string /
// char / strcmp / strcmpi / upper / lower / strtrim / strsplit / strcat /
// strlength / strrep / contains / startsWith / endsWith. The regex
// builtins (regexp/regexpi/regexprep) live in regex.cpp.

#include <numkit/builtin/library.hpp>
#include <numkit/builtin/language/strings/strings.hpp>
#include <numkit/builtin/language/strings/format.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include "helpers.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace numkit::builtin {

// ════════════════════════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════════════════════════

// ── Conversion ──────────────────────────────────────────────────────────

// MATLAB num2str signatures (R2025b):
//   num2str(X)         → ~5 significant digits (default)
//   num2str(X, N)      → N significant digits, where N is integer
//   num2str(X, FMT)    → printf-style format
// See BUGS.md #26.

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

Value num2str(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.type() == ValueType::COMPLEX) {
        if (!x.isScalar())
            throw Error("num2str: complex array formatting (column-aligned) is "
                        "not supported in this revision; only scalar complex",
                        0, 0, "num2str", "", "numkit:num2str:complexArray");
        return Value::fromString(num2strComplexScalar(x.toComplex(), -1), mr);
    }
    if (x.isEmpty()) return Value::fromString("", mr);
    // Real, non-scalar: synthesise MATLAB's default column format and route
    // through the FMT overload (which handles per-row layout, common-blank-
    // column stripping, and NaN/Inf spelling).
    if (!x.isScalar())
        return num2str(x, num2strArrayFormat(x, -1), mr);
    const double v = x.toScalar();
    // MATLAB num2str default precision is MAGNITUDE-AWARE, not a fixed 5 sig
    // figs: it keeps ~4 digits after the integer part, so prec = digits-left-
    // of-decimal + 4 = max(floor(log10|v|),0) + 5. Hence num2str(1000000) =
    // "1000000" and num2str(1000000.5) = "1000000.5" (a fixed "%.5g" wrongly
    // gave "1e+06"). Non-finite values use MATLAB's capitalised spelling.
    if (std::isnan(v)) return Value::fromString("NaN", mr);
    if (std::isinf(v)) return Value::fromString(v < 0 ? "-Inf" : "Inf", mr);
    int prec = 5;
    if (v != 0.0) {
        const int e = static_cast<int>(std::floor(std::log10(std::fabs(v))));
        prec = (e > 0 ? e : 0) + 5;
    }
    char buf[512];
    std::snprintf(buf, sizeof(buf), "%.*g", prec, v);
    return Value::fromString(std::string(buf), mr);
}

Value num2str(const Value &x, int N, std::pmr::memory_resource *mr)
{
    if (x.type() == ValueType::COMPLEX) {
        if (!x.isScalar())
            throw Error("num2str: complex array formatting (column-aligned) is "
                        "not supported in this revision; only scalar complex",
                        0, 0, "num2str", "", "numkit:num2str:complexArray");
        int n = N; if (n < 1) n = 1;
        return Value::fromString(num2strComplexScalar(x.toComplex(), n), mr);
    }
    if (x.isEmpty()) return Value::fromString("", mr);
    // Real, non-scalar with explicit precision N: MATLAB uses a "%<N+7>.<N>g"
    // column field for every element (no integer-detection in the N form).
    if (!x.isScalar())
        return num2str(x, num2strArrayFormat(x, N), mr);
    const double v = x.toScalar();
    int n = N;
    if (n < 1)  n = 1;
    if (n > 99) n = 99;
    char fmt[16];
    std::snprintf(fmt, sizeof(fmt), "%%.%dg", n);
    char buf[256];
    std::snprintf(buf, sizeof(buf), fmt, v);
    return Value::fromString(std::string(buf), mr);
}

Value num2str(const Value &x, const std::string &fmt,
              std::pmr::memory_resource *mr)
{
    // Route the value through the sprintf engine rather than a raw
    // snprintf(fmt, double): the engine handles integer conversions
    // (%d/%i/%u/%o/%x read an int from the va_list, so passing a double
    // straight to snprintf printed garbage — e.g. num2str(5,'%05d') gave
    // "00000" instead of "00005"), the non-integer->%e fallback, and the
    // Inf/NaN spelling. MATLAB then strips the leading AND trailing blank
    // COLUMNS common to all rows (keeping leading zeros and internal
    // spacing) and returns an N-row char matrix:
    //   num2str(pi,'%8.4f')                 -> "3.1416"
    //   num2str(5,'%05d')                   -> "00005"
    //   num2str([1.5 2.25 3.125],'%8.3f')   -> "1.500   2.250   3.125"  (1 row)
    //   num2str([1.5 2.25;3.1 4],'%8.3f')   -> 2x13 char matrix
    // The format is applied cyclically across each ROW (MATLAB sprintf
    // semantics), matching numkit's sprintf which cycles a format over a
    // vector. NOTE: the DEFAULT-precision and integer-N forms for vector/
    // matrix inputs (num2str([1 2 3]) / num2str(v,4)) use MATLAB's
    // magnitude-dependent column-width algorithm and remain a deferred gap
    // (num2str_reg still routes those to the scalar overloads, which throw
    // on non-scalars) — only the deterministic FMT form is handled here.

    // Scalar complex: apply the format to the real and imaginary parts
    // independently and join them re±|im|i (MATLAB num2str(3.14159-2.71828i,
    // '%.3f') -> "3.142-2.718i"). Complex ARRAYS remain a deferred gap.
    if (x.type() == ValueType::COMPLEX) {
        if (!x.isScalar())
            throw Error("num2str: complex array formatting (column-aligned) is "
                        "not supported in this revision; only scalar complex",
                        0, 0, "num2str", "", "numkit:num2str:complexArray");
        const Complex z = x.toComplex();
        const std::string sr = num2str(Value::scalar(z.real(), mr), fmt, mr).toString();
        if (z.imag() == 0.0) return Value::fromString(sr, mr);
        std::string s = sr;
        s += (z.imag() < 0.0 ? '-' : '+');
        s += num2str(Value::scalar(std::fabs(z.imag()), mr), fmt, mr).toString();
        s += 'i';
        return Value::fromString(s, mr);
    }

    const std::size_t nrows = x.dims().rows();
    const std::size_t ncols = x.dims().cols();
    if (nrows * ncols == 0)
        return Value::fromString("", mr);

    Value fmtVal = Value::fromString(fmt, mr);
    ScratchArena scratch(mr);
    auto rows = ScratchVec<std::string>(&scratch);
    rows.reserve(nrows);

    // Build one formatted string per row (format cycled across the row's
    // columns, in column order).
    for (std::size_t r = 0; r < nrows; ++r) {
        Value rowv = Value::matrix(1, ncols, ValueType::DOUBLE, mr);
        double *rd = rowv.doubleDataMut();
        for (std::size_t c = 0; c < ncols; ++c)
            rd[c] = x.elemAsDouble(c * nrows + r);
        Span<const Value> args(&rowv, 1);
        rows.emplace_back(sprintf(fmtVal, args, mr).toString());
    }

    auto leadingWS = [](const std::string &s) {
        std::size_t k = 0;
        while (k < s.size() && (s[k] == ' ' || s[k] == '\t')) ++k;
        return k;
    };
    auto trailingWS = [](const std::string &s) {
        std::size_t k = 0;
        while (k < s.size() && (s[s.size() - 1 - k] == ' ' || s[s.size() - 1 - k] == '\t'))
            ++k;
        return k;
    };

    // Strip the leading/trailing blank columns common to ALL rows (= the
    // minimum leading/trailing whitespace run over the rows). For a scalar
    // this reproduces the old find_first_not_of/find_last_not_of trim.
    std::size_t kLead = std::string::npos, kTrail = std::string::npos;
    for (const auto &s : rows) {
        kLead  = std::min(kLead,  leadingWS(s));
        kTrail = std::min(kTrail, trailingWS(s));
    }
    if (kLead == std::string::npos) kLead = 0;
    if (kTrail == std::string::npos) kTrail = 0;

    std::size_t maxW = 0;
    for (auto &s : rows) {
        const std::size_t len = s.size();
        // A row that is entirely whitespace collapses to empty.
        if (kLead + kTrail >= len) s.clear();
        else s = s.substr(kLead, len - kLead - kTrail);
        maxW = std::max(maxW, s.size());
    }

    if (nrows == 1)
        return Value::fromString(rows[0], mr);

    // Right-pad each row to maxW and emit an nrows x maxW char matrix
    // (column-major). Rows are equal length for fixed-width formats; the
    // pad only matters for variable-width formats.
    auto m = Value::matrix(nrows, maxW, ValueType::CHAR, mr);
    char *dst = static_cast<char *>(m.rawDataMut());
    for (std::size_t r = 0; r < nrows; ++r)
        for (std::size_t c = 0; c < maxW; ++c)
            dst[c * nrows + r] = (c < rows[r].size()) ? rows[r][c] : ' ';
    return m;
}

Value int2str(const Value &x, std::pmr::memory_resource *mr)
{
    // MATLAB int2str: round half away from zero (std::round), render as a
    // plain integer with no decimals or scientific notation. Inf/-Inf/NaN
    // pass through. For complex input MATLAB operates on the REAL part (the
    // imaginary part is discarded): int2str(3.6+1.2i) = "4".

    // Vector / 2-D matrix: round every element FIRST (printf %.0f rounds
    // half-to-even, but int2str rounds half-away-from-zero), then format with
    // a fixed integer field W = digits(max|rounded|) + 2 and route through the
    // num2str FMT path (per-row layout + common-blank-column strip).
    if (!x.isEmpty() && !x.isScalar() && !x.dims().is3D()) {
        const size_t n = x.numel();
        const bool cplx = (x.type() == ValueType::COMPLEX);
        Value rounded = Value::matrix(x.dims().rows(), x.dims().cols(),
                                      ValueType::DOUBLE, mr);
        double *rd = rounded.doubleDataMut();
        double maxabs = 0.0;
        for (size_t i = 0; i < n; ++i) {
            const double vi = cplx ? x.complexData()[i].real() : x.elemAsDouble(i);
            double r = std::isfinite(vi) ? std::round(vi) : vi;
            if (r == 0.0) r = 0.0;   // normalise -0 -> 0
            rd[i] = r;
            if (std::isfinite(r) && std::fabs(r) > maxabs) maxabs = std::fabs(r);
        }
        int ndigits = 1;
        if (maxabs >= 1.0)
            ndigits = static_cast<int>(std::floor(std::log10(maxabs))) + 1;
        char fmt[16];
        std::snprintf(fmt, sizeof(fmt), "%%%d.0f", ndigits + 2);
        return num2str(rounded, std::string(fmt), mr);
    }
    if (x.isEmpty()) return Value::fromString("", mr);

    const double v = (x.type() == ValueType::COMPLEX) ? x.toComplex().real()
                                                      : x.toScalar();
    if (std::isnan(v))
        return Value::fromString("NaN", mr);
    if (std::isinf(v))
        return Value::fromString(v < 0 ? "-Inf" : "Inf", mr);
    double r = std::round(v);
    if (r == 0.0) r = 0.0;          // normalise -0 -> 0
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.0f", r);
    return Value::fromString(std::string(buf), mr);
}

Value validatestring(const Value &str, const Value &valid,
                     std::pmr::memory_resource *mr)
{
    auto lower = [](std::string s) {
        for (auto &c : s)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    };
    const std::string s  = str.toString();
    const std::string sl = lower(s);

    // Candidates (original case) from a cell array, string array, or a lone
    // char vector.
    ScratchArena scratch(mr);
    ScratchVec<std::string> cands(&scratch);
    if (valid.isCell() || valid.isString()) {
        const auto &vec = valid.cellDataVec();
        for (const auto &e : vec) cands.push_back(e.toString());
    } else if (valid.isChar()) {
        cands.push_back(valid.toString());
    } else {
        throw Error("validatestring: second argument must be a cell array of "
                    "char vectors or a string array",
                     0, 0, "validatestring", "", "numkit:validatestring:badList");
    }

    // 1) Exact (case-insensitive) match wins.
    for (const auto &c : cands)
        if (lower(c) == sl)
            return Value::fromString(c, mr);

    // 2) Case-insensitive leading-substring (prefix) matches.
    ScratchVec<const std::string *> pre(&scratch);
    for (const auto &c : cands) {
        const std::string cl = lower(c);
        if (cl.size() >= sl.size() && cl.compare(0, sl.size(), sl) == 0)
            pre.push_back(&c);
    }
    if (pre.empty())
        throw Error("validatestring: '" + s + "' did not match any valid string",
                     0, 0, "validatestring", "", "numkit:validatestring:unrecognized");
    if (pre.size() == 1)
        return Value::fromString(*pre[0], mr);

    // 3) Multiple prefix matches: unambiguous only if the shortest is itself a
    //    leading substring of every other match (then return the shortest).
    const std::string *shortest = pre[0];
    for (auto *p : pre)
        if (p->size() < shortest->size()) shortest = p;
    const std::string shl = lower(*shortest);
    bool prefixOfAll = true;
    for (auto *p : pre) {
        const std::string pl = lower(*p);
        if (!(pl.size() >= shl.size() && pl.compare(0, shl.size(), shl) == 0)) {
            prefixOfAll = false;
            break;
        }
    }
    if (prefixOfAll)
        return Value::fromString(*shortest, mr);
    throw Error("validatestring: '" + s + "' matches multiple valid strings",
                 0, 0, "validatestring", "", "numkit:validatestring:ambiguous");
}

Value str2num(const Value &s, std::pmr::memory_resource *mr)
{
    try {
        return Value::scalar(std::stod(s.toString()), mr);
    } catch (...) {
        return Value();
    }
}

// Parse a numeric substring with strtod; the ENTIRE string must be consumed
// (else NaN). Empty -> defVal. ('42abc' / '42 7' -> NaN; 'Inf'/'-Inf'/'NaN'
// parse.) strtod parses leading "inf"/"nan" case-insensitively.
static double strtodFull(const std::string &t, double defVal)
{
    if (t.empty()) return defVal;
    const char *cs = t.c_str();
    char *end = nullptr;
    const double v = std::strtod(cs, &end);
    if (end == cs || *end != '\0') return std::numeric_limits<double>::quiet_NaN();
    return v;
}

// Parse ONE token the way MATLAB str2double does: strip ALL commas (thousands
// separators) and ALL whitespace (MATLAB tolerates spaces even inside complex
// literals, e.g. ' 2 + 3i '), then the ENTIRE remaining token must parse as a
// single real OR complex number — otherwise NaN. Complex: a trailing lowercase
// 'i' or 'j' marks the imaginary part ('2i', '1+2i', '1-2j', 'i', '-i',
// '3.5+1.5i', '1e-3+2i', 'Infi'); the real/imag split is the LAST '+'/'-' that
// is not an exponent sign. Capital 'I'/'J' are NOT imaginary (-> real parse).
struct Str2DoubleResult { double re; double im; bool isComplex; };

static Str2DoubleResult str2doubleParse(const std::string &str)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();
    auto isWs = [](char c) {
        return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
    };
    // Strip commas (thousands separators); trim SURROUNDING whitespace only.
    std::string t;
    t.reserve(str.size());
    for (char c : str) if (c != ',') t.push_back(c);
    const char *ws = " \t\n\r\f\v";
    const size_t b = t.find_first_not_of(ws);
    if (b == std::string::npos) return {nan, 0.0, false};   // empty / all-whitespace
    const size_t e = t.find_last_not_of(ws);
    t = t.substr(b, e - b + 1);

    const char last = t.back();
    if (last != 'i' && last != 'j')               // pure real
        return { strtodFull(t, nan), 0.0, false };   // internal whitespace -> NaN

    // COMPLEX: MATLAB tolerates whitespace around the +/- and the i/j
    // (e.g. ' 2 + 3i '), so strip ALL remaining whitespace before splitting.
    std::string c2;
    c2.reserve(t.size());
    for (char ch : t) if (!isWs(ch)) c2.push_back(ch);
    const std::string body = c2.substr(0, c2.size() - 1);   // drop the i/j
    // Split at the rightmost +/- at index >= 1 whose predecessor is not e/E
    // (so exponent signs like the '-' in '1e-3' are not treated as a split).
    std::size_t split = std::string::npos;
    for (std::size_t k = body.size(); k-- > 1; ) {
        const char c = body[k];
        if ((c == '+' || c == '-') && body[k - 1] != 'e' && body[k - 1] != 'E') {
            split = k; break;
        }
    }
    auto parseImag = [&](const std::string &is) -> double {
        std::string m = is;
        double sign = 1.0;
        if (!m.empty() && (m[0] == '+' || m[0] == '-')) {
            if (m[0] == '-') sign = -1.0;
            m = m.substr(1);
        }
        if (m.empty()) return sign;               // bare i / +i / -i  -> ±1
        return sign * strtodFull(m, nan);
    };
    double re, im;
    if (split == std::string::npos) { re = 0.0; im = parseImag(body); }
    else { re = strtodFull(body.substr(0, split), nan); im = parseImag(body.substr(split)); }
    return { re, im, true };
}

Value str2double(const Value &s, std::pmr::memory_resource *mr)
{
    const double nan = std::numeric_limits<double>::quiet_NaN();

    // A cell array of char/string vectors OR a non-scalar string array maps
    // element-wise to a matrix of the SAME shape (MATLAB str2double). NaN where
    // an element fails to parse, is empty, or is not char/string. The output is
    // COMPLEX iff ANY element parses as a complex literal (real elements then
    // carry a zero imaginary part); otherwise it stays DOUBLE (zero regression).
    if (s.isCell() || (s.isString() && s.numel() != 1)) {
        const size_t r = static_cast<size_t>(s.dims().rows());
        const size_t c = static_cast<size_t>(s.dims().cols());
        const size_t n = s.numel();
        ScratchArena scratch(mr);
        ScratchVec<Str2DoubleResult> res(n, &scratch);
        bool anyComplex = false;
        for (size_t i = 0; i < n; ++i) {
            if (s.isCell()) {
                const Value &el = s.cellAt(i);
                res[i] = (el.isChar() || el.isString())
                             ? str2doubleParse(el.toString())
                             : Str2DoubleResult{nan, 0.0, false};
            } else {
                res[i] = str2doubleParse(s.stringElem(i));
            }
            anyComplex = anyComplex || res[i].isComplex;
        }
        if (anyComplex) {
            Value out = Value::matrix(r, c, ValueType::COMPLEX, mr);
            Complex *od = out.complexDataMut();
            for (size_t i = 0; i < n; ++i) od[i] = Complex(res[i].re, res[i].im);
            return out;
        }
        Value out = Value::matrix(r, c, ValueType::DOUBLE, mr);
        double *od = out.doubleDataMut();
        for (size_t i = 0; i < n; ++i) od[i] = res[i].re;
        return out;
    }

    // Scalar char array / string scalar → scalar double (or complex scalar).
    const Str2DoubleResult res = str2doubleParse(s.toString());
    if (res.isComplex) {
        Value out = Value::matrix(1, 1, ValueType::COMPLEX, mr);
        out.complexDataMut()[0] = Complex(res.re, res.im);
        return out;
    }
    return Value::scalar(res.re, mr);
}

Value toString(const Value &x, std::pmr::memory_resource *mr)
{
    std::pmr::memory_resource *p = mr;
    if (x.isString())
        return x;
    if (x.isChar())
        return Value::stringScalar(x.toString(), p);
    if (x.isNumeric()) {
        if (x.isScalar()) {
            std::ostringstream os;
            os << x.toScalar();
            return Value::stringScalar(os.str(), p);
        }
        auto result = Value::stringArray(x.dims().rows(), x.dims().cols());
        for (size_t i = 0; i < x.numel(); ++i) {
            std::ostringstream os;
            os << x.doubleData()[i];
            result.stringElemSet(i, os.str());
        }
        return result;
    }
    if (x.isLogical())
        return Value::stringScalar(x.toBool() ? "true" : "false", p);
    if (x.isCell()) {
        // Cell-of-chars / cell-of-strings → string array of same shape.
        // See BUGS.md #4.
        const size_t n = x.numel();
        auto result = Value::stringArray(x.dims().rows(), x.dims().cols(), p);
        for (size_t i = 0; i < n; ++i) {
            const Value &el = x.cellAt(i);
            if (el.isChar() || el.isString())
                result.stringElemSet(i, el.toString());
            else if (el.isNumeric() && el.isScalar()) {
                std::ostringstream os;
                os << el.toScalar();
                result.stringElemSet(i, os.str());
            } else {
                throw Error(
                    "string: cell elements must be char, string, or numeric scalar",
                    0, 0, "string", "", "numkit:string:cellElementType");
            }
        }
        return result;
    }
    throw Error("Cannot convert input to string", 0, 0, "string", "",
                 "numkit:string:unsupportedType");
}

Value toChar(const Value &x, std::pmr::memory_resource *mr)
{
    std::pmr::memory_resource *p = mr;
    if (x.isChar())
        return x;
    if (x.isString())
        return Value::fromString(x.toString(), p);
    if (x.isNumeric()) {
        std::string s;
        if (x.isScalar()) {
            s += static_cast<char>(static_cast<int>(x.toScalar()));
        } else {
            const double *d = x.doubleData();
            for (size_t i = 0; i < x.numel(); ++i)
                s += static_cast<char>(static_cast<int>(d[i]));
        }
        return Value::fromString(s, p);
    }
    throw Error("Cannot convert to char", 0, 0, "char", "", "numkit:char:unsupportedType");
}

// ── Comparisons ─────────────────────────────────────────────────────────

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

Value strcmp(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    return strCmpElementwise(a, b,
        [](const std::string &x, const std::string &y) { return x == y; },
        "strcmp", mr);
}

Value strcmpi(const Value &a, const Value &b, std::pmr::memory_resource *mr)
{
    return strCmpElementwise(a, b,
        [](const std::string &x, const std::string &y) {
            std::string sa = x, sb = y;
            std::transform(sa.begin(), sa.end(), sa.begin(), ::tolower);
            std::transform(sb.begin(), sb.end(), sb.begin(), ::tolower);
            return sa == sb;
        }, "strcmpi", mr);
}

Value strncmp(const Value &a, const Value &b, size_t n, std::pmr::memory_resource *mr)
{
    return strCmpElementwise(a, b,
        [n](const std::string &x, const std::string &y) { return strnEq(x, y, n); },
        "strncmp", mr);
}

Value strncmpi(const Value &a, const Value &b, size_t n, std::pmr::memory_resource *mr)
{
    return strCmpElementwise(a, b,
        [n](const std::string &x, const std::string &y) {
            std::string sa = x, sb = y;
            std::transform(sa.begin(), sa.end(), sa.begin(), ::tolower);
            std::transform(sb.begin(), sb.end(), sb.begin(), ::tolower);
            return strnEq(sa, sb, n);
        }, "strncmpi", mr);
}

// ── Case transforms ─────────────────────────────────────────────────────

// Apply a per-string transform to every element of a CELL array, returning a
// cell of char vectors with the same shape (MATLAB's element-wise behaviour
// for lower/upper/strtrim/deblank/strip). The scalar (non-cell) path is left
// to each caller so the pre-existing scalar return type is preserved exactly.
template <class Op>
static Value mapStringCell(const Value &s, Op op, std::pmr::memory_resource *mr)
{
    const size_t r = static_cast<size_t>(s.dims().rows());
    const size_t c = static_cast<size_t>(s.dims().cols());
    auto out = Value::cell(r, c, mr);
    const size_t n = s.numel();
    for (size_t i = 0; i < n; ++i)
        out.cellAt(i) = Value::fromString(op(s.cellAt(i).toString()), mr);
    return out;
}

// Apply a per-string transform preserving the MATLAB container class:
//   cell           -> cell of char vectors (same shape)
//   string array   -> string array (same shape; a scalar string stays a
//                     1×1 string, NOT a char — strtrim/deblank/strip are
//                     class-preserving in MATLAB)
//   char / other   -> char (the whole array is treated as one token, the
//                     pre-existing scalar behaviour)
template <class Op>
static Value mapStringPreserveClass(const Value &s, Op op, std::pmr::memory_resource *mr)
{
    if (s.isCell()) return mapStringCell(s, op, mr);
    if (s.isString()) {
        const size_t r = static_cast<size_t>(s.dims().rows());
        const size_t c = static_cast<size_t>(s.dims().cols());
        Value out = Value::stringArray(r, c, mr);
        const size_t n = s.numel();
        for (size_t i = 0; i < n; ++i)
            out.stringElemSet(i, op(s.stringElem(i)));
        return out;
    }
    return Value::fromString(op(s.toString()), mr);
}

Value upper(const Value &s, std::pmr::memory_resource *mr)
{
    auto op = [](std::string r) {
        std::transform(r.begin(), r.end(), r.begin(), ::toupper);
        return r;
    };
    return mapStringPreserveClass(s, op, mr);
}

Value lower(const Value &s, std::pmr::memory_resource *mr)
{
    auto op = [](std::string r) {
        std::transform(r.begin(), r.end(), r.begin(), ::tolower);
        return r;
    };
    return mapStringPreserveClass(s, op, mr);
}

// ── Trim / split / concat ───────────────────────────────────────────────

Value strtrim(const Value &s, std::pmr::memory_resource *mr)
{
    auto op = [](const std::string &r) -> std::string {
        size_t start = r.find_first_not_of(" \t\r\n");
        size_t end = r.find_last_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        return r.substr(start, end - start + 1);
    };
    return mapStringPreserveClass(s, op, mr);
}

Value deblank(const Value &s, std::pmr::memory_resource *mr)
{
    auto op = [](const std::string &r) -> std::string {
        size_t end = r.find_last_not_of(" \t\r\n\f\v");
        if (end == std::string::npos) return "";
        return r.substr(0, end + 1);
    };
    return mapStringPreserveClass(s, op, mr);
}

Value blanks(size_t n, std::pmr::memory_resource *mr)
{
    return Value::fromString(std::string(n, ' '), mr);
}

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

Value strsplit(const Value &s, std::pmr::memory_resource *mr)
{
    ScratchArena scratch(mr);
    ScratchVec<std::string> delims(&scratch);
    appendDefaultWhitespace(delims);
    return strsplitImpl(s.toString(), delims, /*collapse=*/true, mr);
}

Value strsplit(const Value &s, const Value &delim, std::pmr::memory_resource *mr)
{
    ScratchArena scratch(mr);
    ScratchVec<std::string> delims(&scratch);
    appendDelims(delim, delims);
    return strsplitImpl(s.toString(), delims, /*collapse=*/true, mr);
}

Value strcat(Span<const Value> parts, std::pmr::memory_resource *mr)
{
    std::string result;
    for (const auto &p : parts)
        result += p.toString();
    return Value::fromString(result, mr);
}

// ── Length ──────────────────────────────────────────────────────────────

Value strlength(const Value &s, std::pmr::memory_resource *mr)
{
    std::pmr::memory_resource *p = mr;
    if (s.isString()) {
        if (s.isScalar())
            return Value::scalar(static_cast<double>(s.toString().size()), p);
        auto result = createLike(s, ValueType::DOUBLE, p);
        for (size_t i = 0; i < s.numel(); ++i)
            result.doubleDataMut()[i] = static_cast<double>(s.stringElem(i).size());
        return result;
    }
    if (s.isChar())
        return Value::scalar(static_cast<double>(s.numel()), p);
    throw Error("Input must be a string or char array", 0, 0, "strlength", "",
                 "numkit:strlength:unsupportedType");
}

// ── Search / replace ────────────────────────────────────────────────────

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

Value strfind(const Value &s, const Value &pat, std::pmr::memory_resource *mr)
{
    const std::string pp = pat.toString();
    // A cell / non-scalar string array source -> same-shape CELL of index
    // vectors (MATLAB); a scalar char / string -> a 1×k DOUBLE row vector.
    if (s.isCell() || (s.isString() && !s.isScalar())) {
        const size_t r = static_cast<size_t>(s.dims().rows());
        const size_t c = static_cast<size_t>(s.dims().cols());
        Value out = Value::cell(r, c, mr);
        const size_t n = s.numel();
        for (size_t i = 0; i < n; ++i) {
            const std::string el =
                s.isCell() ? s.cellAt(i).toString() : s.stringElem(i);
            out.cellAt(i) = strfindOne(el, pp, mr);
        }
        return out;
    }
    return strfindOne(s.toString(), pp, mr);
}

Value mat2str(const Value &x, int precision, std::pmr::memory_resource *mr)
{
    // CHAR: render a quoted char literal (MATLAB R2025b). A char row ->
    // 'abc'; a multi-row char matrix -> ['ab';'cd']; empty char '' -> "''".
    // Internal single quotes are doubled ('a''b'). Checked before the empty
    // guard so '' produces "''" rather than "[]".
    if (x.isChar()) {
        if (x.dims().ndim() > 2)
            throw Error("mat2str: only 2-D inputs are supported",
                         0, 0, "mat2str", "", "numkit:mat2str:rank");
        const size_t R = x.dims().rows(), C = x.dims().cols();
        const char *cd = (x.numel() > 0) ? x.charData() : nullptr;
        auto quoteRow = [&](size_t r) {
            std::string s;
            s.push_back('\'');
            for (size_t c = 0; c < C; ++c) {
                const char ch = cd[c * R + r];   // col-major
                if (ch == '\'') s.push_back('\'');   // double internal quote
                s.push_back(ch);
            }
            s.push_back('\'');
            return s;
        };
        if (R <= 1)
            return Value::fromString(quoteRow(0), mr);
        std::string out;
        out.push_back('[');
        for (size_t r = 0; r < R; ++r) {
            if (r > 0) out.push_back(';');
            out += quoteRow(r);
        }
        out.push_back(']');
        return Value::fromString(out, mr);
    }

    if (x.isEmpty())
        return Value::fromString("[]", mr);

    const auto &d = x.dims();
    if (d.ndim() > 2)
        throw Error("mat2str: only 2-D inputs are supported",
                     0, 0, "mat2str", "", "numkit:mat2str:rank");
    const size_t R = d.rows(), C = d.cols();

    // Logical: elements print as the words true / false (MATLAB R2025b),
    // e.g. mat2str(true) -> "true", mat2str([true false]) -> "[true false]".
    if (x.isLogical()) {
        auto fmtL = [](double v) -> const char * {
            return v != 0.0 ? "true" : "false";
        };
        if (x.isScalar())
            return Value::fromString(fmtL(x.elemAsDouble(0)), mr);
        std::string out;
        out.push_back('[');
        for (size_t r = 0; r < R; ++r) {
            if (r > 0) out.push_back(';');
            for (size_t c = 0; c < C; ++c) {
                if (c > 0) out.push_back(' ');
                out += fmtL(x.elemAsDouble(c * R + r));
            }
        }
        out.push_back(']');
        return Value::fromString(out, mr);
    }

    auto fmt = [precision](double v) {
        if (v == 0.0) v = 0.0;   // normalize -0 → 0 (MATLAB never prints "-0")
        std::ostringstream os;
        os.precision(precision);
        os << v;
        return os.str();
    };

    // Complex: format each element INDEPENDENTLY (matches MATLAB mat2str) —
    // an element with exactly-zero imaginary part prints as a bare real, even
    // when other elements of the same array are complex. E.g.
    // mat2str(complex([5 3],[0 4])) -> "[5 3+4i]" (NOT "[5+0i 3+4i]"); an
    // all-zero-imag complex array therefore prints fully real ("[1 2]").
    if (x.type() == ValueType::COMPLEX) {
        const Complex *cd = x.complexData();
        const size_t n = x.numel();
        auto fmtC = [&](const Complex &z) -> std::string {
            const double im = z.imag();
            if (im == 0.0) return fmt(z.real());
            std::string s = fmt(z.real());
            s += (im < 0.0 ? '-' : '+');
            s += fmt(im < 0.0 ? -im : im);
            s += 'i';
            return s;
        };
        if (x.isScalar()) return Value::fromString(fmtC(cd[0]), mr);
        std::string out;
        out.reserve(n * (precision + 8) + R + 4);
        out.push_back('[');
        for (size_t r = 0; r < R; ++r) {
            if (r > 0) out.push_back(';');
            for (size_t c = 0; c < C; ++c) {
                if (c > 0) out.push_back(' ');
                out += fmtC(cd[c * R + r]);
            }
        }
        out.push_back(']');
        return Value::fromString(out, mr);
    }

    // Real numeric (double / single / int8..uint64). Read via elemAsDouble so
    // integer and single arrays format the same way MATLAB mat2str does:
    // bare values with no class wrapper (mat2str(int8([1 2])) -> "[1 2]").
    if (x.isScalar()) {
        return Value::fromString(fmt(x.elemAsDouble(0)), mr);
    }

    std::string out;
    out.reserve(R * C * (precision + 4) + R + 4);
    out.push_back('[');
    for (size_t r = 0; r < R; ++r) {
        if (r > 0) out.push_back(';');
        for (size_t c = 0; c < C; ++c) {
            if (c > 0) out.push_back(' ');
            out += fmt(x.elemAsDouble(c * R + r));
        }
    }
    out.push_back(']');
    return Value::fromString(out, mr);
}

Value strjoin(const Value &c, const Value &delim, std::pmr::memory_resource *mr)
{
    if (!c.isCell())
        throw Error("strjoin: first argument must be a cell array",
                     0, 0, "strjoin", "", "numkit:strjoin:notCell");
    const size_t n = c.numel();
    std::string out;
    if (delim.isCell()) {
        // MATLAB R2025b: a cell array of numel(C)-1 delimiters, interleaved
        // between consecutive elements: strjoin({'a','b','c'},{', ',' and '})
        // -> 'a, b and c'.
        const size_t nd = delim.numel();
        if (n > 0 && nd != n - 1)
            throw Error("strjoin: delimiter cell array must have one fewer "
                         "element than the first argument",
                         0, 0, "strjoin", "", "numkit:strjoin:badDelimCount");
        for (size_t i = 0; i < n; ++i) {
            if (i > 0) out += delim.cellAt(i - 1).toString();
            out += c.cellAt(i).toString();
        }
    } else {
        const std::string sep =
            delim.isEmpty() ? std::string(" ") : delim.toString();
        for (size_t i = 0; i < n; ++i) {
            if (i > 0) out += sep;
            out += c.cellAt(i).toString();
        }
    }
    return Value::fromString(out, mr);
}

// ── Pack 18 ──────────────────────────────────────────────────────────

Value append(Span<const Value> parts, std::pmr::memory_resource *mr)
{
    // Same shape as strcat but does NOT trim trailing whitespace from
    // char-array operands (MATLAB's "append" is the literal-concatenate
    // variant that script users reach for).
    std::string out;
    for (const auto &p : parts)
        out += p.toString();
    return Value::fromString(out, mr);
}

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

Value count(const Value &s, const Value &pat, std::pmr::memory_resource *mr)
{
    // pat may be a single pattern or a cell/string array of patterns; MATLAB
    // sums the non-overlapping occurrence counts across all listed patterns.
    // A cell str input is processed element-wise, returning a DOUBLE array the
    // same shape as the cell (counts per element).
    ScratchArena scratch(mr);
    ScratchVec<std::string> pats(&scratch);
    collectMatchPatterns(pat, pats);
    auto countOne = [&](const std::string &ss) -> double {
        size_t n = 0;
        for (const auto &pp : pats) {
            if (pp.empty()) continue;
            size_t pos = 0;
            while ((pos = ss.find(pp, pos)) != std::string::npos) {
                ++n;
                pos += pp.size();   // non-overlapping (matches MATLAB)
            }
        }
        return static_cast<double>(n);
    };
    if (s.isCell()) {
        const size_t r = static_cast<size_t>(s.dims().rows());
        const size_t c = static_cast<size_t>(s.dims().cols());
        auto out = Value::matrix(r, c, ValueType::DOUBLE, mr);
        double *od = out.doubleDataMut();
        const size_t nn = s.numel();
        for (size_t i = 0; i < nn; ++i) od[i] = countOne(s.cellAt(i).toString());
        return out;
    }
    // A string ARRAY is processed element-wise -> DOUBLE array, same shape.
    if (s.isString() && !s.isScalar()) {
        const size_t r = static_cast<size_t>(s.dims().rows());
        const size_t c = static_cast<size_t>(s.dims().cols());
        auto out = Value::matrix(r, c, ValueType::DOUBLE, mr);
        double *od = out.doubleDataMut();
        const size_t nn = s.numel();
        for (size_t i = 0; i < nn; ++i) od[i] = countOne(s.stringElem(i));
        return out;
    }
    return Value::scalar(countOne(s.toString()), mr);
}

Value erase(const Value &s, const Value &pat, std::pmr::memory_resource *mr)
{
    // pat may be a single pattern or a cell/string array; MATLAB removes every
    // occurrence of each listed pattern, applied in order. A cell str input is
    // processed element-wise, returning a cell of char vectors (same shape).
    ScratchArena scratch(mr);
    ScratchVec<std::string> pats(&scratch);
    collectMatchPatterns(pat, pats);
    auto op = [&](std::string r) -> std::string {
        for (const auto &pp : pats) {
            if (pp.empty()) continue;
            size_t pos = 0;
            while ((pos = r.find(pp, pos)) != std::string::npos)
                r.erase(pos, pp.size());
        }
        return r;
    };
    return mapStringPreserveClass(s, op, mr);
}

Value replace(const Value &s, const Value &oldPat, const Value &newPat, std::pmr::memory_resource *mr)
{
    // oldPat/newPat may each be a single pattern OR a cell / string array.
    // MATLAB scans the source once, left to right: at each position the FIRST
    // old pattern (in list order) that matches there is replaced with its
    // corresponding new text, then the scan advances past the matched source
    // (no re-scanning, no chain-replacement). A single NEW applies to every
    // OLD; otherwise NEW must pair 1:1 with OLD.
    const std::string ss = s.toString();
    ScratchArena scratch(mr);
    ScratchVec<std::string> olds(&scratch), news(&scratch);
    collectMatchPatterns(oldPat, olds);
    collectMatchPatterns(newPat, news);
    if (news.size() != 1 && news.size() != olds.size())
        throw Error("replace: NEW must be a scalar text or match the number of "
                    "OLD patterns",
                     0, 0, "replace", "", "numkit:replace:sizeMismatch");

    std::string out;
    out.reserve(ss.size());
    size_t i = 0;
    while (i < ss.size()) {
        bool matched = false;
        for (size_t k = 0; k < olds.size(); ++k) {
            const std::string &o = olds[k];
            if (!o.empty() && i + o.size() <= ss.size()
                && ss.compare(i, o.size(), o) == 0) {
                out += (news.size() == 1) ? news[0] : news[k];
                i += o.size();
                matched = true;
                break;
            }
        }
        if (!matched) {
            out += ss[i];
            ++i;
        }
    }
    if (s.isString()) return Value::stringScalar(out, mr);
    return Value::fromString(out, mr);
}

Value reverse(const Value &s, std::pmr::memory_resource *mr)
{
    auto op = [](std::string r) {
        std::reverse(r.begin(), r.end());
        return r;
    };
    return mapStringPreserveClass(s, op, mr);
}

Value splitlines(const Value &s, std::pmr::memory_resource *mr)
{
    const std::string ss = s.toString();
    ScratchArena scratch(mr);
    ScratchVec<std::string> parts(&scratch);
    std::string cur;
    for (size_t i = 0; i < ss.size(); ++i) {
        const char c = ss[i];
        if (c == '\r') {
            parts.push_back(std::move(cur));
            cur.clear();
            // Skip a following \n to handle CRLF as one separator.
            if (i + 1 < ss.size() && ss[i + 1] == '\n') ++i;
        } else if (c == '\n') {
            parts.push_back(std::move(cur));
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    // Always push the final segment, even if empty (matches MATLAB:
    // splitlines("a\nb") returns 2 entries; splitlines("a\n") returns 2).
    parts.push_back(std::move(cur));
    auto out = Value::cell(parts.size(), 1, mr);
    for (size_t i = 0; i < parts.size(); ++i)
        out.cellAt(i) = Value::fromString(parts[i], mr);
    return out;
}

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

// Pad ONE string to width n on the given side with char ch. MATLAB 'both'
// splits the extra padding floor(pad/2) left, ceil(pad/2) right.
static std::string padOne(std::string r, size_t n, const std::string &sd, char ch)
{
    if (r.size() >= n) return r;
    const size_t pad = n - r.size();
    if (sd == "left") {
        r.insert(r.begin(), pad, ch);
    } else if (sd == "both") {
        const size_t left = pad / 2;
        r.insert(r.begin(), left, ch);
        r.append(pad - left, ch);
    } else {  // "right" (default)
        r.append(pad, ch);
    }
    return r;
}

Value pad(const Value &s, size_t n, const Value &side, const Value &padChar, std::pmr::memory_resource *mr)
{
    const std::string sd = readSide(side, "right");
    if (sd != "left" && sd != "right" && sd != "both")
        throw Error("pad: side must be 'left', 'right', or 'both'",
                     0, 0, "pad", "", "numkit:pad:badSide");
    char ch = ' ';
    if (!padChar.isEmpty() && (padChar.isChar() || padChar.isString())) {
        const auto p = padChar.toString();
        if (!p.empty()) ch = p[0];
    }
    // string array -> string array same shape; char -> char; cell -> cell
    // of char vectors. Each element is padded to the SAME width n.
    auto op = [&](std::string r) { return padOne(std::move(r), n, sd, ch); };
    return mapStringPreserveClass(s, op, mr);
}

Value strip(const Value &s, const Value &side, const Value &ch, std::pmr::memory_resource *mr)
{
    const std::string sd = readSide(side, "both");
    std::string charsToStrip = " \t\r\n\f\v";
    bool noStrip = false;  // explicit empty strip-set => no-op
    if (!ch.isEmpty() && (ch.isChar() || ch.isString())) {
        charsToStrip = ch.toString();
        if (charsToStrip.empty()) noStrip = true;
    }
    auto stripOne = [&](std::string r) -> std::string {
        if (noStrip) return r;
        if (sd == "left" || sd == "both") {
            size_t i = 0;
            while (i < r.size() && charsToStrip.find(r[i]) != std::string::npos) ++i;
            if (i > 0) r.erase(0, i);
        }
        if (sd == "right" || sd == "both") {
            while (!r.empty() && charsToStrip.find(r.back()) != std::string::npos)
                r.pop_back();
        }
        return r;
    };
    return mapStringPreserveClass(s, stripOne, mr);
}

Value matches(const Value &s, const Value &pat, std::pmr::memory_resource *mr)
{
    // The pattern may be a single string or a cell / string array of
    // alternatives; an element matches if it equals ANY of them.
    ScratchArena scratch(mr);
    ScratchVec<std::string> pats(&scratch);
    collectMatchPatterns(pat, pats);
    auto matchOne = [&](const std::string &el) -> bool {
        for (const auto &pp : pats)
            if (el == pp) return true;
        return false;
    };
    // A string ARRAY / cell source -> same-shape LOGICAL array (element-wise).
    if (s.isCell() || (s.isString() && !s.isScalar())) {
        const size_t r = static_cast<size_t>(s.dims().rows());
        const size_t c = static_cast<size_t>(s.dims().cols());
        Value out = Value::matrix(r, c, ValueType::LOGICAL, mr);
        uint8_t *od = out.logicalDataMut();
        const size_t nn = s.numel();
        for (size_t i = 0; i < nn; ++i) {
            const std::string el =
                s.isCell() ? s.cellAt(i).toString() : s.stringElem(i);
            od[i] = matchOne(el) ? 1 : 0;
        }
        return out;
    }
    return Value::logicalScalar(matchOne(s.toString()), mr);
}

// ── Pack 21 ──────────────────────────────────────────────────────────

Value convertCharsToStrings(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isString()) return x;
    if (x.isChar())   return Value::stringScalar(x.toString(), mr);
    if (x.isCell()) {
        const auto &d = x.dims();
        auto c = d.is3D()
                    ? Value::cell3D(d.rows(), d.cols(), d.pages(), mr)
                    : Value::cell(d.rows(), d.cols(), mr);
        for (size_t i = 0; i < x.numel(); ++i) {
            const auto &e = x.cellAt(i);
            if (e.isChar())
                c.cellAt(i) = Value::stringScalar(e.toString(), mr);
            else
                c.cellAt(i) = e;
        }
        return c;
    }
    // Numeric / other types: pass through unchanged.
    return x;
}

Value convertStringsToChars(const Value &x, std::pmr::memory_resource *mr)
{
    if (x.isChar()) return x;
    if (x.isString()) {
        if (x.numel() <= 1) {
            return Value::fromString(x.toString(), mr);
        }
        // Multi-element string → cell of char rows.
        const auto &d = x.dims();
        auto c = d.is3D()
                    ? Value::cell3D(d.rows(), d.cols(), d.pages(), mr)
                    : Value::cell(d.rows(), d.cols(), mr);
        for (size_t i = 0; i < x.numel(); ++i)
            c.cellAt(i) = Value::fromString(x.stringElem(i), mr);
        return c;
    }
    if (x.isCell()) {
        const auto &d = x.dims();
        auto c = d.is3D()
                    ? Value::cell3D(d.rows(), d.cols(), d.pages(), mr)
                    : Value::cell(d.rows(), d.cols(), mr);
        for (size_t i = 0; i < x.numel(); ++i) {
            const auto &e = x.cellAt(i);
            if (e.isString())
                c.cellAt(i) = Value::fromString(e.toString(), mr);
            else
                c.cellAt(i) = e;
        }
        return c;
    }
    return x;
}

Value isstringscalar(const Value &x, std::pmr::memory_resource *mr)
{
    return Value::logicalScalar(x.isString() && x.numel() == 1, mr);
}

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

Value isstrprop(const Value &s, const Value &category, std::pmr::memory_resource *mr)
{
    if (!category.isChar() && !category.isString())
        throw Error("isstrprop: category must be a string",
                     0, 0, "isstrprop", "", "numkit:isstrprop:cat");
    auto cat = category.toString();
    for (auto &c : cat) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (cat == "alpha")
        return applyCharPred(s, [](unsigned char c) { return std::isalpha(c) != 0; }, mr);
    if (cat == "digit")
        return applyCharPred(s, [](unsigned char c) { return std::isdigit(c) != 0; }, mr);
    if (cat == "alphanum")
        return applyCharPred(s, [](unsigned char c) { return std::isalnum(c) != 0; }, mr);
    if (cat == "lower")
        return applyCharPred(s, [](unsigned char c) { return std::islower(c) != 0; }, mr);
    if (cat == "upper")
        return applyCharPred(s, [](unsigned char c) { return std::isupper(c) != 0; }, mr);
    if (cat == "punct")
        return applyCharPred(s, [](unsigned char c) { return std::ispunct(c) != 0; }, mr);
    if (cat == "space" || cat == "wspace")
        return applyCharPred(s, [](unsigned char c) { return std::isspace(c) != 0; }, mr);
    if (cat == "xdigit")
        return applyCharPred(s, [](unsigned char c) { return std::isxdigit(c) != 0; }, mr);
    if (cat == "cntrl")
        return applyCharPred(s, [](unsigned char c) { return std::iscntrl(c) != 0; }, mr);
    if (cat == "graphic")
        return applyCharPred(s, [](unsigned char c) { return std::isgraph(c) != 0; }, mr);
    if (cat == "print")
        return applyCharPred(s, [](unsigned char c) { return std::isprint(c) != 0; }, mr);
    throw Error("isstrprop: unknown category '" + cat + "'",
                 0, 0, "isstrprop", "", "numkit:isstrprop:badCat");
}

Value isletter(const Value &s, std::pmr::memory_resource *mr)
{
    return applyCharPred(s, [](unsigned char c) { return std::isalpha(c) != 0; }, mr);
}

Value isspaceFn(const Value &s, std::pmr::memory_resource *mr)
{
    return applyCharPred(s, [](unsigned char c) { return std::isspace(c) != 0; }, mr);
}

// ── Pack 22 ──────────────────────────────────────────────────────────
//
// Position-or-pattern lookup: numeric scalar → 0-based index, string
// → find first occurrence and return its [begin, end) range.

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

Value extractAfter(const Value &s, const Value &p, std::pmr::memory_resource *mr)
{
    // A cell str is processed element-wise (cell of char vectors); the
    // position/substring anchor p is scalar and applies to every element.
    auto op = [&](const std::string &ss) -> std::string {
        const auto r = resolvePos(ss, p);
        return r.found ? ss.substr(r.end) : std::string();
    };
    return mapStringPreserveClass(s, op, mr);
}

Value extractBefore(const Value &s, const Value &p, std::pmr::memory_resource *mr)
{
    auto op = [&](const std::string &ss) -> std::string {
        const auto r = resolvePos(ss, p);
        return r.found ? ss.substr(0, r.begin) : std::string();
    };
    return mapStringPreserveClass(s, op, mr);
}

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

Value extractBetween(const Value &s, const Value &start, const Value &end, std::pmr::memory_resource *mr)
{
    const std::string ss = s.toString();
    const auto matches = findAllBetweenPairs(ss, start, end);
    // MATLAB returns an M-by-1 cell of inner strings — even for a
    // single match. Empty result is a 0-by-1 cell. See BUGS.md #27.
    auto c = Value::cell(matches.size(), 1, mr);
    for (size_t i = 0; i < matches.size(); ++i) {
        const auto &m = matches[i];
        const std::string inner = ss.substr(m.openEnd, m.closeBegin - m.openEnd);
        c.cellAt(i) = strLikeOf(s, inner, mr);
    }
    return c;
}

Value insertAfter(const Value &s, const Value &p, const Value &newText, std::pmr::memory_resource *mr)
{
    // A cell str is processed element-wise (cell of char vectors); the anchor
    // p and inserted text are scalar and apply to every element. An element
    // with no match is left unchanged.
    const std::string nt = newText.toString();
    auto op = [&](std::string ss) -> std::string {
        const auto r = resolvePos(ss, p);
        if (r.found) ss.insert(r.end, nt);
        return ss;
    };
    return mapStringPreserveClass(s, op, mr);
}

Value insertBefore(const Value &s, const Value &p, const Value &newText, std::pmr::memory_resource *mr)
{
    const std::string nt = newText.toString();
    auto op = [&](std::string ss) -> std::string {
        const auto r = resolvePos(ss, p);
        if (r.found) ss.insert(r.begin, nt);
        return ss;
    };
    return mapStringPreserveClass(s, op, mr);
}

Value eraseBetween(const Value &s, const Value &start, const Value &end, std::pmr::memory_resource *mr)
{
    const std::string ss = s.toString();
    const auto matches = findAllBetweenPairs(ss, start, end);
    if (matches.empty()) return s;
    // Walk left-to-right copying segments outside the matched ranges.
    // See BUGS.md #27.
    std::string out;
    out.reserve(ss.size());
    size_t cursor = 0;
    for (const auto &m : matches) {
        out.append(ss, cursor, m.openEnd - cursor);   // up to and including opening delim
        cursor = m.closeBegin;                          // skip the inner text
    }
    out.append(ss, cursor, ss.size() - cursor);
    return strLikeOf(s, out, mr);
}

Value replaceBetween(const Value &s, const Value &start, const Value &end, const Value &newText, std::pmr::memory_resource *mr)
{
    const std::string ss = s.toString();
    const std::string nt = newText.toString();
    const auto matches = findAllBetweenPairs(ss, start, end);
    if (matches.empty()) return s;
    std::string out;
    out.reserve(ss.size());
    size_t cursor = 0;
    for (const auto &m : matches) {
        out.append(ss, cursor, m.openEnd - cursor);   // copy through opening delim
        out.append(nt);                                // replacement text
        cursor = m.closeBegin;                          // resume at closing delim
    }
    out.append(ss, cursor, ss.size() - cursor);
    return strLikeOf(s, out, mr);
}

// ── Pack 23 ──────────────────────────────────────────────────────────

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

Value dec2bin(const Value &d, int minWidth, std::pmr::memory_resource *mr)
{
    return vecToBaseMatrix(d, 2, minWidth, mr);
}

Value dec2hex(const Value &d, int minWidth, std::pmr::memory_resource *mr)
{
    return vecToBaseMatrix(d, 16, minWidth, mr);
}

Value bin2dec(const Value &s, std::pmr::memory_resource *mr)
{
    return Value::scalar(static_cast<double>(parseBase(s.toString(), 2)), mr);
}

Value hex2dec(const Value &s, std::pmr::memory_resource *mr)
{
    return Value::scalar(static_cast<double>(parseBase(s.toString(), 16)), mr);
}

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

Value hex2num(const Value &s, std::pmr::memory_resource *mr)
{
    // A char MATRIX (>1 row) parses each row as one number -> N×1 column.
    const size_t rows = static_cast<size_t>(s.dims().rows());
    if (s.type() == ValueType::CHAR && rows > 1) {
        const size_t cols = s.numel() / rows;
        const char *src = static_cast<const char *>(s.rawData());
        Value out = Value::matrix(rows, 1, ValueType::DOUBLE, mr);
        double *od = out.doubleDataMut();
        for (size_t r = 0; r < rows; ++r) {
            std::string row;
            row.reserve(cols);
            for (size_t c = 0; c < cols; ++c) row.push_back(src[c * rows + r]);
            od[r] = hexBitsToDouble(row);
        }
        return out;
    }
    // A cellstr / string array -> same-shape double array.
    if (s.isCell() || (s.isString() && !s.isScalar())) {
        const size_t n = s.numel();
        Value out = Value::matrix(s.dims().rows(), s.dims().cols(),
                                  ValueType::DOUBLE, mr);
        double *od = out.doubleDataMut();
        for (size_t i = 0; i < n; ++i) {
            const std::string str =
                s.isCell() ? s.cellAt(i).toString() : s.stringElem(i);
            od[i] = hexBitsToDouble(str);
        }
        return out;
    }
    // Scalar char row / string scalar -> scalar double.
    return Value::scalar(hexBitsToDouble(s.toString()), mr);
}

Value num2hex(const Value &x, std::pmr::memory_resource *mr)
{
    const ValueType t = x.type();
    const bool single = (t == ValueType::SINGLE);
    const bool dbl    = (t == ValueType::DOUBLE);
    if (!single && !dbl)
        throw Error("num2hex: inputs must be floating point (single or double)",
                     0, 0, "num2hex", "", "numkit:num2hex:notFloat");

    const size_t n = x.numel();
    const int W = single ? 8 : 16;
    if (n == 0) return Value::matrix(0, static_cast<size_t>(W), ValueType::CHAR, mr);

    // numel × W CHAR matrix, one row per element (column-major order).
    Value m = Value::matrix(n, static_cast<size_t>(W), ValueType::CHAR, mr);
    char *dst = static_cast<char *>(m.rawDataMut());
    char buf[24];
    for (size_t i = 0; i < n; ++i) {
        const double v = x.elemAsDouble(i);
        if (single) {
            const float f = static_cast<float>(v);
            uint32_t bits;
            std::memcpy(&bits, &f, sizeof(bits));
            std::snprintf(buf, sizeof(buf), "%08x", static_cast<unsigned int>(bits));
        } else {
            uint64_t bits;
            std::memcpy(&bits, &v, sizeof(bits));
            std::snprintf(buf, sizeof(buf), "%016llx",
                          static_cast<unsigned long long>(bits));
        }
        for (int c = 0; c < W; ++c)
            dst[static_cast<size_t>(c) * n + i] = buf[c];
    }
    return m;
}

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

Value dec2base(const Value &d, int base, int minWidth, std::pmr::memory_resource *mr)
{
    if (base < 2 || base > 36)
        throw Error("dec2base: base must be in 2..36",
                     0, 0, "dec2base", "", "numkit:dec2base:badBase");
    return vecToBaseMatrix(d, base, minWidth, mr);
}

Value base2dec(const Value &s, int base, std::pmr::memory_resource *mr)
{
    if (base < 2 || base > 36)
        throw Error("base2dec: base must be in 2..36",
                     0, 0, "base2dec", "", "numkit:base2dec:badBase");
    const size_t rows = static_cast<size_t>(s.dims().rows());
    // A char MATRIX (multiple rows) parses each row -> column vector.
    if (rows > 1 && s.type() == ValueType::CHAR) {
        const size_t cols = s.numel() / rows;
        const char *src = static_cast<const char *>(s.rawData());
        Value out = Value::matrix(rows, 1, ValueType::DOUBLE, mr);
        double *od = out.doubleDataMut();
        for (size_t r = 0; r < rows; ++r) {
            std::string row;
            row.reserve(cols);
            for (size_t c = 0; c < cols; ++c) row.push_back(src[c * rows + r]);
            od[r] = static_cast<double>(parseBaseN(row, base));
        }
        return out;
    }
    return Value::scalar(static_cast<double>(parseBaseN(s.toString(), base)), mr);
}

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

// Single-element CF expansion, used by both 1-output (string form) and
// 2-output (numeric N, D) callsites.
Value rat(const Value &x, double tol, std::pmr::memory_resource *mr)
{
    const double v = x.toScalar();
    if (!std::isfinite(v))
        return Value::fromString(std::isnan(v) ? "NaN" : (v > 0 ? "Inf" : "-Inf"), mr);
    if (tol <= 0.0) tol = defaultRatTol(v);
    const auto exp = ratExpansion(v, tol);
    return Value::fromString(buildCFString(exp), mr);
}

// rats — fixed-width formatted ratio. MATLAB renders each element as
// `numerator/denominator` and pads to `len` characters per element.
// Default len = 13 per the MATLAB doc, BUT the rendered field is actually
// `len + 1` characters wide — MATLAB reserves one extra column for a
// leading sign on negative values (kept as space when positive). Field
// width = len + 1 here matches `strlength(rats(0.5))` = 14 in MATLAB R2025b.
// For a vector input MATLAB concatenates the per-element fields into one
// big string row.
Value rats(const Value &x, int len, std::pmr::memory_resource *mr)
{
    if (len <= 0) len = 13;
    const int field_width = len + 1;  // MATLAB reserves 1 col for leading sign
    const size_t n = x.numel();

    // Layout per MATLAB R2025b: numerator is right-justified in the first
    // half of the field (cols 1..field_width/2), '/' sits at the boundary
    // (col field_width/2 + 1), denominator is left-justified in the second
    // half. Empirically: rats(0.5) → '      1/2     ' (numer at col 7,
    // slash at col 8). For integer denominator (q == 1) the whole number
    // is right-justified in the full field.
    const int num_width = field_width / 2;          // 7 for default len=13
    const int den_width = field_width - num_width - 1;  // 6 for default
    auto formatOne = [&](double v) -> std::string {
        if (!std::isfinite(v)) {
            const std::string s = std::isnan(v) ? "NaN" : (v > 0 ? "Inf" : "-Inf");
            const int slack = field_width - static_cast<int>(s.size());
            return std::string(std::max(0, slack), ' ') + s;
        }
        const double tol = defaultRatTol(v);
        const auto exp = ratExpansion(v, tol);
        const std::string ns = std::to_string(exp.p);
        if (exp.q == 1) {
            const int slack = field_width - static_cast<int>(ns.size());
            return std::string(std::max(0, slack), ' ') + ns;
        }
        const std::string ds = std::to_string(exp.q);
        const int n_pad = std::max(0, num_width - static_cast<int>(ns.size()));
        const int d_pad = std::max(0, den_width - static_cast<int>(ds.size()));
        return std::string(n_pad, ' ') + ns + "/" + ds + std::string(d_pad, ' ');
    };

    if (n <= 1) {
        const double v = x.toScalar();
        return Value::fromString(formatOne(v), mr);
    }

    // Vector: concatenate per-element fields end-to-end (one row).
    std::string out;
    out.reserve(n * static_cast<size_t>(field_width));
    for (size_t i = 0; i < n; ++i)
        out += formatOne(x.elemAsDouble(i));
    return Value::fromString(out, mr);
}

// Single-string literal replacement: replace every non-overlapping occurrence
// of `op` in `r0` with `np`. Empty `op` is a no-op (matches MATLAB).
static std::string strrepOne(const std::string &r0, const std::string &op,
                             const std::string &np)
{
    std::string r = r0;
    if (!op.empty()) {
        size_t pos = 0;
        while ((pos = r.find(op, pos)) != std::string::npos) {
            r.replace(pos, op.length(), np);
            pos += np.length();
        }
    }
    return r;
}

Value strrep(const Value &s, const Value &oldPat, const Value &newPat, std::pmr::memory_resource *mr)
{
    std::pmr::memory_resource *p = mr;
    const bool sc = s.isCell(), oc = oldPat.isCell(), nc = newPat.isCell();

    // Scalar path: no cell arguments — return a char (or string) scalar,
    // preserving the original behaviour exactly.
    if (!sc && !oc && !nc) {
        std::string r = strrepOne(s.toString(), oldPat.toString(), newPat.toString());
        if (s.isString())
            return Value::stringScalar(r, p);
        return Value::fromString(r, p);
    }

    // Cell-aware path (MATLAB: any cell-array argument => cell output). Cell
    // operands must share a common size, or be scalar (1x1) and broadcast;
    // non-cell char/string arguments broadcast to every element. The result
    // is a cell of char vectors shaped like the non-scalar cell operand.
    auto numelOf = [](const Value &v, bool isCellArg) -> size_t {
        return isCellArg ? v.numel() : size_t{1};
    };
    const size_t ns = numelOf(s, sc), no = numelOf(oldPat, oc), nn = numelOf(newPat, nc);
    size_t n = 1;
    for (size_t v : {ns, no, nn}) {
        if (v == 1) continue;
        if (n == 1) n = v;
        else if (v != n)
            throw Error("strrep: nonscalar arguments must match in size",
                        0, 0, "strrep", "", "numkit:strrep:cellSize");
    }
    size_t rr = 1, cc = 1;
    auto setShape = [&](const Value &v) {
        rr = static_cast<size_t>(v.dims().rows());
        cc = static_cast<size_t>(v.dims().cols());
    };
    // Shape comes from the non-scalar cell operand if any, else the (1x1)
    // cell operand that triggered the cell path.
    if (sc && ns == n && n != 1)       setShape(s);
    else if (oc && no == n && n != 1)  setShape(oldPat);
    else if (nc && nn == n && n != 1)  setShape(newPat);
    else if (sc)                       setShape(s);
    else if (oc)                       setShape(oldPat);
    else                               setShape(newPat);

    const std::string ss0 = sc ? std::string() : s.toString();
    const std::string os0 = oc ? std::string() : oldPat.toString();
    const std::string ns0 = nc ? std::string() : newPat.toString();

    auto out = Value::cell(rr, cc, p);
    for (size_t i = 0; i < n; ++i) {
        const std::string si = sc ? s.cellAt(ns == 1 ? 0 : i).toString() : ss0;
        const std::string oi = oc ? oldPat.cellAt(no == 1 ? 0 : i).toString() : os0;
        const std::string ni = nc ? newPat.cellAt(nn == 1 ? 0 : i).toString() : ns0;
        out.cellAt(i) = Value::fromString(strrepOne(si, oi, ni), p);
    }
    return out;
}

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

Value contains(const Value &s, const Value &pat, bool ignoreCase, std::pmr::memory_resource *mr)
{
    return strPredicate(s, pat, ignoreCase, StrPred::Contains, mr);
}

Value startsWith(const Value &s, const Value &prefix, bool ignoreCase, std::pmr::memory_resource *mr)
{
    return strPredicate(s, prefix, ignoreCase, StrPred::StartsWith, mr);
}

Value endsWith(const Value &s, const Value &suffix, bool ignoreCase, std::pmr::memory_resource *mr)
{
    return strPredicate(s, suffix, ignoreCase, StrPred::EndsWith, mr);
}

// ── Pack 36: compose / strjust / extract / split / join ──────────────

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

Value compose(const Value &fmt, const Value &x, std::pmr::memory_resource *mr)
{
    if (!fmt.isChar() && !fmt.isString())
        throw Error("compose: format must be a char or string",
                     0, 0, "compose", "", "numkit:compose:badFmt");
    const std::string fmtStr = fmt.toString();
    // MATLAB: the output class mirrors the FORMAT class — a string format
    // yields a string array, a char format yields a cell of char vectors.
    const bool wantString = fmt.isString();

    // M = number of value-consuming conversion specs in one pass of the
    // format. MATLAB applies the format repeatedly across each ROW of x,
    // consuming M values per output element, so an R×C input yields an
    // R×ceil(C/M) result. A short trailing chunk leaves the unfilled specs
    // as literal text (e.g. compose('%d-%d',[1 2 3]) -> "1-2","3-%d").
    const size_t M = std::max<size_t>(1, countFormatSpecs(fmtStr));

    const auto &dims = x.dims();
    const size_t R = dims.rows();
    const size_t C = dims.cols();
    const size_t outCols = (C == 0) ? 0 : (C + M - 1) / M;

    auto buildResult = [&](size_t rows, size_t cols) -> Value {
        return wantString ? Value::stringArray(rows, cols, mr)
                          : Value::cell(rows, cols, mr);
    };
    auto setElem = [&](Value &v, size_t idx, const std::string &s) {
        if (wantString) v.stringElemSet(idx, s);
        else            v.cellAt(idx) = Value::fromString(s, mr);
    };

    if (R == 0 || C == 0)
        return buildResult(R, outCols);

    Value result = buildResult(R, outCols);

    std::vector<Value> chunk;
    chunk.reserve(M);
    for (size_t r = 0; r < R; ++r) {
        for (size_t j = 0; j < outCols; ++j) {
            chunk.clear();
            for (size_t k = 0; k < M; ++k) {
                const size_t col = j * M + k;
                if (col >= C) break;  // short trailing chunk
                // Column-major linear index of x(r, col).
                chunk.push_back(elemScalar(x, col * R + r, mr));
            }
            const std::string s =
                formatOnce(fmtStr, {chunk.data(), chunk.size()}, 0,
                           /*literalWhenShort=*/true);
            setElem(result, j * R + r, s);  // column-major out(r, j)
        }
    }
    return result;
}

Value strjust(const Value &M, const std::string &side, std::pmr::memory_resource *mr)
{
    if (!M.isChar())
        throw Error("strjust: input must be a char matrix",
                     0, 0, "strjust", "", "numkit:strjust:badInput");
    const auto &dims = M.dims();
    const size_t rows = dims.rows();
    const size_t cols = dims.cols();
    const std::string src = M.toString();
    if (rows == 0 || cols == 0) return M;

    std::string out(rows * cols, ' ');
    for (size_t r = 0; r < rows; ++r) {
        // Column-major: row r, col c -> src[c*rows + r].
        size_t firstNonSp = cols, lastNonSp = 0;
        bool hasNonSp = false;
        for (size_t c = 0; c < cols; ++c) {
            char ch = src[c * rows + r];
            if (ch != ' ') {
                if (!hasNonSp) { firstNonSp = c; hasNonSp = true; }
                lastNonSp = c;
            }
        }
        if (!hasNonSp) continue;  // whole row blank

        const size_t len = lastNonSp - firstNonSp + 1;
        size_t target;
        if      (side == "left")   target = 0;
        else if (side == "center") target = (cols - len) / 2;
        else if (side == "right")  target = cols - len;
        else throw Error("strjust: side must be 'left', 'right', or 'center'",
                          0, 0, "strjust", "", "numkit:strjust:badSide");

        for (size_t c = 0; c < len; ++c)
            out[(target + c) * rows + r] = src[(firstNonSp + c) * rows + r];
    }

    // Build a char matrix of the same shape as M.
    Value r = Value::matrix(rows, cols, ValueType::CHAR, mr);
    char *p = r.charDataMut();
    std::memcpy(p, out.data(), rows * cols);
    return r;
}

Value extract(const Value &s, const Value &pat, std::pmr::memory_resource *mr)
{
    const std::string sStr = strInput(s);
    const std::string pStr = strInput(pat);

    std::vector<std::string> hits;
    findAllLiteral(sStr, pStr, hits);

    if (hits.empty()) return Value::cell(0, 0, mr);
    Value c = Value::cell(hits.size(), 1, mr);
    for (size_t i = 0; i < hits.size(); ++i)
        c.cellAt(i) = Value::fromString(hits[i], mr);
    return c;
}

Value split(const Value &s, const Value &delim, std::pmr::memory_resource *mr)
{
    const std::string sStr = strInput(s);
    const std::string dStr = strInput(delim);

    std::vector<std::string> parts;
    splitKeepEmpty(sStr, dStr, parts);

    // MATLAB: a STRING input yields a string-array result; a char or cellstr
    // input yields a cell array of character vectors. The values and shape
    // (an N×1 column) are identical either way — only the container differs.
    if (s.isString()) {
        Value r = Value::stringArray(parts.size(), 1, mr);
        for (size_t i = 0; i < parts.size(); ++i)
            r.stringElemSet(i, parts[i]);
        return r;
    }

    Value c = Value::cell(parts.size(), 1, mr);
    for (size_t i = 0; i < parts.size(); ++i)
        c.cellAt(i) = Value::fromString(parts[i], mr);
    return c;
}

Value join(const Value &arr, const Value &delim, std::pmr::memory_resource *mr)
{
    const std::string d = delim.isEmpty() ? std::string(" ") : strInput(delim);

    auto getElem = [&](size_t i) -> std::string {
        if (arr.isString()) return arr.stringElem(i);
        if (arr.isCell())   return arr.cellAt(i).toString();
        return arr.toString();  // scalar char/string fallback
    };

    if (arr.isScalar()) {
        return Value::stringScalar(getElem(0), mr);
    }

    const auto &dims = arr.dims();
    const size_t rows = dims.rows();
    const size_t cols = dims.cols();

    if (rows == 1 || cols == 1) {
        // 1-D: glue all elements with `d`.
        std::string out;
        const size_t n = arr.numel();
        for (size_t i = 0; i < n; ++i) {
            if (i > 0) out += d;
            out += getElem(i);
        }
        return Value::stringScalar(out, mr);
    }

    // 2-D: join along columns -> N×1 string column.
    Value r = Value::stringArray(rows, 1, mr);
    for (size_t row = 0; row < rows; ++row) {
        std::string out;
        for (size_t col = 0; col < cols; ++col) {
            if (col > 0) out += d;
            out += getElem(col * rows + row);
        }
        r.stringElemSet(row, out);
    }
    return r;
}

// ── Pack 36: array constructors / character constants ───────────────
Value newlineFn(std::pmr::memory_resource *mr)
{
    return Value::fromString("\n", mr);
}

Value stringsND(Span<const size_t> dims, std::pmr::memory_resource *mr)
{
    const size_t ndim = dims.size();
    if (ndim == 0)
        return Value::stringScalar("", mr);

    const size_t r = dims[0];
    const size_t c = (ndim >= 2) ? dims[1] : r;  // strings(n) → n×n.
    size_t p = 0;
    if (ndim >= 3) {
        p = 1;
        for (size_t i = 2; i < ndim; ++i)
            p *= dims[i];  // collapse extras into pages (3D cap).
    }

    auto v = (p == 0) ? Value::stringArray(r, c, mr)
                      : Value::stringArray3D(r, c, p, mr);
    const size_t n = v.numel();
    for (size_t i = 0; i < n; ++i)
        v.stringElemSet(i, "");
    return v;
}

// strtok: first token + remainder. See language/strings/strings.hpp.
std::pair<Value, Value> strtok(const Value &str, const std::string &delim,
                               std::pmr::memory_resource *mr)
{
    const std::string s = str.toString();
    auto isDelim = [&](char c) { return delim.find(c) != std::string::npos; };

    // Skip leading delim chars.
    size_t start = 0;
    while (start < s.size() && isDelim(s[start])) ++start;
    // Find end of token.
    size_t end = start;
    while (end < s.size() && !isDelim(s[end])) ++end;

    Value tok = Value::fromString(s.substr(start, end - start), mr);
    Value rem = Value::fromString(end < s.size() ? s.substr(end) : std::string{},
                                  mr);
    return {std::move(tok), std::move(rem)};
}

// ════════════════════════════════════════════════════════════════════════
// Adapters
// ════════════════════════════════════════════════════════════════════════

namespace detail {

void num2str_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("num2str: requires 1 argument", 0, 0, "num2str", "", "numkit:num2str:nargin");
    auto *mr = ctx.engine->resource();
    if (args.size() < 2) {
        outs[0] = num2str(args[0], mr);
        return;
    }
    const Value &spec = args[1];
    if (spec.isChar() || spec.isString())
        outs[0] = num2str(args[0], spec.toString(), mr);
    else
        outs[0] = num2str(args[0], static_cast<int>(spec.toScalar()), mr);
}

void int2str_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("int2str: requires 1 argument", 0, 0, "int2str", "", "numkit:int2str:nargin");
    outs[0] = int2str(args[0], ctx.engine->resource());
}

void validatestring_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    // validatestring(str, validStrings [, funcName, varName, position]).
    // The trailing args only customise the error text; they don't change the
    // match, so we accept and ignore them.
    if (args.size() < 2)
        throw Error("validatestring: requires at least 2 arguments (str, validStrings)",
                     0, 0, "validatestring", "", "numkit:validatestring:nargin");
    outs[0] = validatestring(args[0], args[1], ctx.engine->resource());
}

void str2num_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("str2num: requires 1 argument", 0, 0, "str2num", "", "numkit:str2num:nargin");
    // MATLAB str2num evaluates the (bracket-wrapped) string as an expression,
    // so it parses matrices, ranges and arithmetic — str2num('[1 2;3 4]')=
    // [1 2;3 4], '1:5'=1..5, '2+3'=5. Any parse/eval failure (or a non-numeric
    // result) yields [] (0x0 double); the optional 2nd output is a logical
    // success flag: [X, tf] = str2num(s). (The engine-free Value str2num()
    // overload remains a scalar-only fallback for embedders without eval.)
    bool ok = false;
    Value result = Value::Empty;
    if (args[0].isChar() || args[0].isString()) {
        try {
            Value v = ctx.engine->eval("[" + args[0].toString() + "]",
                                       /*suppressTopLevelDisplay=*/true);
            if (v.isNumeric() || v.isLogical()) {
                result = std::move(v);
                ok = true;
            }
        } catch (...) {
            ok = false;
        }
    }
    outs[0] = ok ? std::move(result) : Value::Empty;
    if (nargout >= 2 && outs.size() >= 2)
        outs[1] = Value::logicalScalar(ok, ctx.engine->resource());
}

void str2double_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("str2double: requires 1 argument", 0, 0, "str2double", "",
                     "numkit:str2double:nargin");
    outs[0] = str2double(args[0], ctx.engine->resource());
}

void string_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    std::pmr::memory_resource *mr = ctx.engine->resource();
    if (args.empty()) {
        outs[0] = Value::stringScalar("", mr);
        return;
    }
    outs[0] = toString(args[0], mr);
}

void char_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("char requires an argument", 0, 0, "char", "", "numkit:char:nargin");
    outs[0] = toChar(args[0], ctx.engine->resource());
}

void strcmp_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("strcmp: requires 2 arguments", 0, 0, "strcmp", "", "numkit:strcmp:nargin");
    outs[0] = strcmp(args[0], args[1], ctx.engine->resource());
}

void strcmpi_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("strcmpi: requires 2 arguments", 0, 0, "strcmpi", "",
                     "numkit:strcmpi:nargin");
    outs[0] = strcmpi(args[0], args[1], ctx.engine->resource());
}

void upper_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("upper: requires 1 argument", 0, 0, "upper", "", "numkit:upper:nargin");
    outs[0] = upper(args[0], ctx.engine->resource());
}

void lower_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("lower: requires 1 argument", 0, 0, "lower", "", "numkit:lower:nargin");
    outs[0] = lower(args[0], ctx.engine->resource());
}

void strtrim_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("strtrim: requires 1 argument", 0, 0, "strtrim", "",
                     "numkit:strtrim:nargin");
    outs[0] = strtrim(args[0], ctx.engine->resource());
}

void strsplit_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("strsplit: requires 1 argument", 0, 0, "strsplit", "",
                     "numkit:strsplit:nargin");
    auto *mr = ctx.engine->resource();
    ScratchArena scratch(mr);
    ScratchVec<std::string> delims(&scratch);
    bool collapse = true;
    // arg 1 (optional) is the delimiter: a string or cell array of strings.
    // Name-Value pairs (CollapseDelimiters) follow at arg 2+.
    size_t optStart = 1;
    if (args.size() >= 2) {
        appendDelims(args[1], delims);
        optStart = 2;
    } else {
        appendDefaultWhitespace(delims);
    }
    for (size_t k = optStart; k + 1 < args.size(); k += 2) {
        std::string name = args[k].toString();
        std::transform(name.begin(), name.end(), name.begin(),
                       [](unsigned char ch) { return std::tolower(ch); });
        if (name == "collapsedelimiters")
            collapse = !args[k + 1].isEmpty() && args[k + 1].toScalar() != 0.0;
        // DelimiterType='RegularExpression' is not supported (literal only).
    }
    if (nargout >= 2) {
        // [tokens, matches] = strsplit(...): also return the matched
        // delimiters (1×(numel(tokens)-1) cell of strings).
        ScratchVec<std::string> matched(&scratch);
        outs[0] = strsplitImpl(args[0].toString(), delims, collapse, mr, &matched);
        auto mc = Value::cell(1, matched.size());
        for (size_t k = 0; k < matched.size(); ++k)
            mc.cellAt(k) = Value::fromString(matched[k], mr);
        outs[1] = mc;
    } else {
        outs[0] = strsplitImpl(args[0].toString(), delims, collapse, mr);
    }
}

void strcat_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    outs[0] = strcat(args, ctx.engine->resource());
}

void strlength_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("strlength: requires 1 argument", 0, 0, "strlength", "",
                     "numkit:strlength:nargin");
    outs[0] = strlength(args[0], ctx.engine->resource());
}

void strrep_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("strrep requires 3 arguments", 0, 0, "strrep", "", "numkit:strrep:nargin");
    outs[0] = strrep(args[0], args[1], args[2], ctx.engine->resource());
}

// Parse a trailing 'IgnoreCase', <logical> name-value pair (MATLAB option
// shared by contains/startsWith/endsWith). Other name-value pairs are left
// alone. Returns the flag; absent → false.
static bool parseIgnoreCaseOption(Span<const Value> args, size_t optStart)
{
    for (size_t i = optStart; i + 1 < args.size(); ++i) {
        if (!args[i].isChar() && !args[i].isString()) continue;
        std::string k = args[i].toString();
        for (char &c : k) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (k == "ignorecase")
            return args[i + 1].toBool();
    }
    return false;
}

void contains_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("contains requires 2 arguments", 0, 0, "contains", "",
                     "numkit:contains:nargin");
    outs[0] = contains(args[0], args[1], parseIgnoreCaseOption(args, 2),
                       ctx.engine->resource());
}

void startsWith_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("startsWith requires 2 arguments", 0, 0, "startsWith", "",
                     "numkit:startsWith:nargin");
    outs[0] = startsWith(args[0], args[1], parseIgnoreCaseOption(args, 2),
                         ctx.engine->resource());
}

void endsWith_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("endsWith requires 2 arguments", 0, 0, "endsWith", "",
                     "numkit:endsWith:nargin");
    outs[0] = endsWith(args[0], args[1], parseIgnoreCaseOption(args, 2),
                       ctx.engine->resource());
}

void strncmp_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("strncmp: requires 3 arguments", 0, 0, "strncmp", "",
                     "numkit:strncmp:nargin");
    const size_t n = static_cast<size_t>(args[2].toScalar());
    outs[0] = strncmp(args[0], args[1], n, ctx.engine->resource());
}

void strncmpi_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("strncmpi: requires 3 arguments", 0, 0, "strncmpi", "",
                     "numkit:strncmpi:nargin");
    const size_t n = static_cast<size_t>(args[2].toScalar());
    outs[0] = strncmpi(args[0], args[1], n, ctx.engine->resource());
}

void strfind_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("strfind: requires 2 arguments", 0, 0, "strfind", "",
                     "numkit:strfind:nargin");
    outs[0] = strfind(args[0], args[1], ctx.engine->resource());
}

void blanks_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("blanks: requires 1 argument", 0, 0, "blanks", "",
                     "numkit:blanks:nargin");
    const size_t n = static_cast<size_t>(args[0].toScalar());
    outs[0] = blanks(n, ctx.engine->resource());
}

void deblank_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("deblank: requires 1 argument", 0, 0, "deblank", "",
                     "numkit:deblank:nargin");
    outs[0] = deblank(args[0], ctx.engine->resource());
}

void mat2str_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("mat2str: requires at least 1 argument", 0, 0, "mat2str", "",
                     "numkit:mat2str:nargin");
    auto *mr = ctx.engine->resource();
    int prec = 15;
    bool withClass = false;
    // Trailing args (in any order): a numeric precision, and/or the literal
    // 'class' flag which wraps the output with the class name
    // (mat2str(int8([1 2]),'class') -> "int8([1 2])").
    for (size_t k = 1; k < args.size(); ++k) {
        const Value &a = args[k];
        if (a.type() == ValueType::CHAR || a.isString()) {
            std::string s = a.toString();
            std::transform(s.begin(), s.end(), s.begin(),
                           [](unsigned char ch) { return std::tolower(ch); });
            if (s == "class") withClass = true;
        } else if (!a.isEmpty()) {
            prec = static_cast<int>(a.toScalar());
        }
    }
    Value r = mat2str(args[0], prec, mr);
    if (withClass) {
        std::string wrapped = std::string(mtypeName(args[0].type())) + "(" +
                              r.toString() + ")";
        r = Value::fromString(wrapped, mr);
    }
    outs[0] = r;
}

void strjoin_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("strjoin: requires at least 1 argument", 0, 0, "strjoin", "",
                     "numkit:strjoin:nargin");
    const Value &delim = (args.size() >= 2) ? args[1] : Value::Empty;
    outs[0] = strjoin(args[0], delim, ctx.engine->resource());
}

void append_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    outs[0] = append(args, ctx.engine->resource());
}

void count_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("count: requires (s, pat)",
                     0, 0, "count", "", "numkit:count:nargin");
    outs[0] = count(args[0], args[1], ctx.engine->resource());
}

void erase_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("erase: requires (s, pat)",
                     0, 0, "erase", "", "numkit:erase:nargin");
    outs[0] = erase(args[0], args[1], ctx.engine->resource());
}

void replace_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("replace: requires (s, old, new)",
                     0, 0, "replace", "", "numkit:replace:nargin");
    outs[0] = replace(args[0], args[1], args[2], ctx.engine->resource());
}

void reverse_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("reverse: requires 1 argument",
                     0, 0, "reverse", "", "numkit:reverse:nargin");
    outs[0] = reverse(args[0], ctx.engine->resource());
}

void splitlines_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("splitlines: requires 1 argument",
                     0, 0, "splitlines", "", "numkit:splitlines:nargin");
    outs[0] = splitlines(args[0], ctx.engine->resource());
}

// Default pad width: the longest element of a cell str, or the length of a
// char/string scalar (so pad(s) with no width is a no-op for a scalar but
// right-pads every cell element to the longest, matching MATLAB).
static size_t defaultPadWidth(const Value &s)
{
    if (s.isCell()) {
        size_t mx = 0;
        const size_t nn = s.numel();
        for (size_t i = 0; i < nn; ++i)
            mx = std::max(mx, s.cellAt(i).toString().size());
        return mx;
    }
    if (s.isString()) {
        // A string array defaults to the longest element (s.toString() would
        // collapse the array to its first element).
        size_t mx = 0;
        const size_t nn = s.numel();
        for (size_t i = 0; i < nn; ++i)
            mx = std::max(mx, s.stringElem(i).size());
        return mx;
    }
    return s.toString().size();
}

void pad_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("pad: requires (s[, n][, side[, ch]])",
                     0, 0, "pad", "", "numkit:pad:nargin");
    // The 2nd arg is the width n when numeric; a string there is the side
    // (with the width defaulting to the longest element). pad(s) with no
    // 2nd arg also uses the default width.
    const bool haveN = args.size() >= 2 && !args[1].isEmpty()
                       && !args[1].isChar() && !args[1].isString();
    size_t n, sideIdx, chIdx;
    if (haveN) {
        n = static_cast<size_t>(args[1].toScalar());
        sideIdx = 2;
        chIdx = 3;
    } else {
        n = defaultPadWidth(args[0]);
        sideIdx = 1;   // a string 2nd arg is the side
        chIdx = 2;
    }
    const Value &side = (args.size() > sideIdx && !args[sideIdx].isEmpty()) ? args[sideIdx] : Value::Empty;
    const Value &ch   = (args.size() > chIdx   && !args[chIdx].isEmpty())   ? args[chIdx]   : Value::Empty;
    outs[0] = pad(args[0], n, side, ch, ctx.engine->resource());
}

void strip_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("strip: requires (s[, side[, ch]])",
                     0, 0, "strip", "", "numkit:strip:nargin");
    const Value &side = (args.size() >= 2 && !args[1].isEmpty()) ? args[1] : Value::Empty;
    const Value &ch   = (args.size() >= 3 && !args[2].isEmpty()) ? args[2] : Value::Empty;
    outs[0] = strip(args[0], side, ch, ctx.engine->resource());
}

void matches_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("matches: requires (s, pat)",
                     0, 0, "matches", "", "numkit:matches:nargin");
    outs[0] = matches(args[0], args[1], ctx.engine->resource());
}

void convertCharsToStrings_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("convertCharsToStrings: requires 1 argument",
                     0, 0, "convertCharsToStrings", "", "numkit:convertCharsToStrings:nargin");
    outs[0] = convertCharsToStrings(args[0], ctx.engine->resource());
}

void convertStringsToChars_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("convertStringsToChars: requires 1 argument",
                     0, 0, "convertStringsToChars", "", "numkit:convertStringsToChars:nargin");
    outs[0] = convertStringsToChars(args[0], ctx.engine->resource());
}

void isstringscalar_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("isstringscalar: requires 1 argument",
                     0, 0, "isstringscalar", "", "numkit:isstringscalar:nargin");
    outs[0] = isstringscalar(args[0], ctx.engine->resource());
}

void isstrprop_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("isstrprop: requires (s, category)",
                     0, 0, "isstrprop", "", "numkit:isstrprop:nargin");
    outs[0] = isstrprop(args[0], args[1], ctx.engine->resource());
}

void isletter_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("isletter: requires 1 argument",
                     0, 0, "isletter", "", "numkit:isletter:nargin");
    outs[0] = isletter(args[0], ctx.engine->resource());
}

void isspace_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("isspace: requires 1 argument",
                     0, 0, "isspace", "", "numkit:isspace:nargin");
    outs[0] = isspaceFn(args[0], ctx.engine->resource());
}

#define NK_STR_BIN_REG(FN)                                                       \
    void FN##_reg(Span<const Value> args, size_t, Span<Value> outs,             \
                  CallContext &ctx)                                              \
    {                                                                             \
        if (args.size() < 2)                                                      \
            throw Error(#FN " requires 2 arguments",                             \
                         0, 0, #FN, "", "numkit:" #FN ":nargin");                      \
        outs[0] = FN(args[0], args[1], ctx.engine->resource());                  \
    }

NK_STR_BIN_REG(extractAfter)
NK_STR_BIN_REG(extractBefore)

#undef NK_STR_BIN_REG

void extractBetween_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 3)
        throw Error("extractBetween requires (s, start, end)",
                     0, 0, "extractBetween", "", "numkit:extractBetween:nargin");
    outs[0] = extractBetween(args[0], args[1], args[2], ctx.engine->resource());
}

#define NK_STR_TRI_REG(FN)                                                       \
    void FN##_reg(Span<const Value> args, size_t, Span<Value> outs,             \
                  CallContext &ctx)                                              \
    {                                                                             \
        if (args.size() < 3)                                                      \
            throw Error(#FN " requires 3 arguments",                             \
                         0, 0, #FN, "", "numkit:" #FN ":nargin");                      \
        outs[0] = FN(args[0], args[1], args[2], ctx.engine->resource());         \
    }

NK_STR_TRI_REG(insertAfter)
NK_STR_TRI_REG(insertBefore)
NK_STR_TRI_REG(eraseBetween)

#undef NK_STR_TRI_REG

void replaceBetween_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 4)
        throw Error("replaceBetween requires (s, start, end, new)",
                     0, 0, "replaceBetween", "", "numkit:replaceBetween:nargin");
    outs[0] = replaceBetween(args[0], args[1], args[2], args[3], ctx.engine->resource());
}

void dec2bin_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("dec2bin requires (d[, n])",
                     0, 0, "dec2bin", "", "numkit:dec2bin:nargin");
    int n = (args.size() >= 2 && !args[1].isEmpty())
              ? static_cast<int>(args[1].toScalar()) : 0;
    outs[0] = dec2bin(args[0], n, ctx.engine->resource());
}

void dec2hex_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("dec2hex requires (d[, n])",
                     0, 0, "dec2hex", "", "numkit:dec2hex:nargin");
    int n = (args.size() >= 2 && !args[1].isEmpty())
              ? static_cast<int>(args[1].toScalar()) : 0;
    outs[0] = dec2hex(args[0], n, ctx.engine->resource());
}

void bin2dec_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("bin2dec requires 1 argument",
                     0, 0, "bin2dec", "", "numkit:bin2dec:nargin");
    outs[0] = bin2dec(args[0], ctx.engine->resource());
}

void hex2dec_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("hex2dec requires 1 argument",
                     0, 0, "hex2dec", "", "numkit:hex2dec:nargin");
    outs[0] = hex2dec(args[0], ctx.engine->resource());
}

void hex2num_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("hex2num requires 1 argument",
                     0, 0, "hex2num", "", "numkit:hex2num:nargin");
    outs[0] = hex2num(args[0], ctx.engine->resource());
}

void num2hex_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("num2hex requires 1 argument",
                     0, 0, "num2hex", "", "numkit:num2hex:nargin");
    outs[0] = num2hex(args[0], ctx.engine->resource());
}

void dec2base_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("dec2base requires (d, base[, len])",
                     0, 0, "dec2base", "", "numkit:dec2base:nargin");
    const int base = static_cast<int>(args[1].toScalar());
    const int len  = (args.size() >= 3 && !args[2].isEmpty())
                       ? static_cast<int>(args[2].toScalar()) : 0;
    outs[0] = dec2base(args[0], base, len, ctx.engine->resource());
}

void base2dec_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("base2dec requires (s, base)",
                     0, 0, "base2dec", "", "numkit:base2dec:nargin");
    const int base = static_cast<int>(args[1].toScalar());
    outs[0] = base2dec(args[0], base, ctx.engine->resource());
}

void rat_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("rat requires (x[, tol])",
                     0, 0, "rat", "", "numkit:rat:nargin");
    auto *mr = ctx.engine->resource();
    const Value &x = args[0];
    const double user_tol = (args.size() >= 2 && !args[1].isEmpty())
                            ? args[1].toScalar() : -1.0;  // -1 ⇒ defaultRatTol per element

    if (nargout <= 1) {
        // 1-output form: continued-fraction string. MATLAB's vector
        // version returns a multi-line char array; numkit only emits
        // the scalar form for now (matrix-of-strings is a separate
        // edge — never hit by realistic call sites).
        outs[0] = rat(x, user_tol > 0 ? user_tol : 0.0, mr);
        return;
    }

    // 2-output form: [N, D] = rat(x[, tol]) — numeric, vectorised.
    const auto &dims = x.dims();
    const size_t n = x.numel();
    Value N = dims.is3D()
        ? Value::matrix3d(dims.rows(), dims.cols(), dims.pages(), ValueType::DOUBLE, mr)
        : Value::matrix(dims.rows(), dims.cols(), ValueType::DOUBLE, mr);
    Value D = dims.is3D()
        ? Value::matrix3d(dims.rows(), dims.cols(), dims.pages(), ValueType::DOUBLE, mr)
        : Value::matrix(dims.rows(), dims.cols(), ValueType::DOUBLE, mr);
    double *Nd = N.doubleDataMut();
    double *Dd = D.doubleDataMut();
    for (size_t i = 0; i < n; ++i) {
        const double v = x.elemAsDouble(i);
        if (!std::isfinite(v)) {
            // MATLAB returns NaN/NaN for NaN input, ±Inf/0 for ±Inf? Empirically
            // [n, d] = rat(NaN) → n=NaN, d=NaN; [n, d] = rat(Inf) → n=Inf, d=1.
            if (std::isnan(v)) {
                Nd[i] = std::numeric_limits<double>::quiet_NaN();
                Dd[i] = std::numeric_limits<double>::quiet_NaN();
            } else {
                Nd[i] = v;  // ±Inf
                Dd[i] = 1.0;
            }
            continue;
        }
        const double tol = (user_tol > 0) ? user_tol : defaultRatTol(v);
        const auto exp = ratExpansion(v, tol);
        Nd[i] = static_cast<double>(exp.p);
        Dd[i] = static_cast<double>(exp.q);
    }
    outs[0] = std::move(N);
    outs[1] = std::move(D);
}

void rats_reg(Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("rats requires (x[, len])",
                     0, 0, "rats", "", "numkit:rats:nargin");
    int len = (args.size() >= 2 && !args[1].isEmpty())
                  ? static_cast<int>(args[1].toScalar()) : 13;
    outs[0] = rats(args[0], len, ctx.engine->resource());
}

// strtok(s, delim?) — split at first delim char. Returns [token, rem].
// Default delim is ASCII whitespace.
void strtok_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    if (args.empty())
        throw Error("strtok: requires 1 argument", 0, 0, "strtok", "",
                     "numkit:strtok:nargin");
    const std::string delim = (args.size() >= 2)
                                  ? args[1].toString()
                                  : std::string(" \t\r\n\f\v");
    auto [tok, rem] = numkit::builtin::strtok(args[0], delim,
                                              ctx.engine->resource());
    outs[0] = std::move(tok);
    if (nargout > 1) outs[1] = std::move(rem);
}

// ── Pack 36 adapters ─────────────────────────────────────────────────
void newline_reg(Span<const Value> /*args*/, size_t, Span<Value> outs,
                 CallContext &ctx)
{
    outs[0] = newlineFn(ctx.engine->resource());
}

void strings_reg(Span<const Value> args, size_t, Span<Value> outs,
                 CallContext &ctx)
{
    auto *mr = ctx.engine->resource();
    ScratchArena scratch(mr);
    auto d = parseDimsArgsND(&scratch, args);
    outs[0] = stringsND(Span<const size_t>(d.data(), d.size()), mr);
}

void compose_reg(Span<const Value> args, size_t, Span<Value> outs,
                 CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("compose: requires 2 arguments (fmt, x)",
                     0, 0, "compose", "", "numkit:compose:nargin");
    outs[0] = compose(args[0], args[1], ctx.engine->resource());
}

void strjust_reg(Span<const Value> args, size_t, Span<Value> outs,
                 CallContext &ctx)
{
    if (args.empty())
        throw Error("strjust: requires at least 1 argument",
                     0, 0, "strjust", "", "numkit:strjust:nargin");
    std::string side = "right";
    if (args.size() >= 2) {
        if (!args[1].isChar() && !args[1].isString())
            throw Error("strjust: side must be a char or string",
                         0, 0, "strjust", "", "numkit:strjust:badSide");
        side = args[1].toString();
    }
    outs[0] = strjust(args[0], side, ctx.engine->resource());
}

void extract_reg(Span<const Value> args, size_t, Span<Value> outs,
                 CallContext &ctx)
{
    if (args.size() < 2)
        throw Error("extract: requires 2 arguments (s, pat)",
                     0, 0, "extract", "", "numkit:extract:nargin");
    outs[0] = extract(args[0], args[1], ctx.engine->resource());
}

void split_reg(Span<const Value> args, size_t, Span<Value> outs,
               CallContext &ctx)
{
    if (args.empty())
        throw Error("split: requires at least 1 argument",
                     0, 0, "split", "", "numkit:split:nargin");
    auto *mr = ctx.engine->resource();
    if (args.size() == 1) {
        // Default delimiter is whitespace per MATLAB; we use ' '.
        Value sp = Value::fromString(" ", mr);
        outs[0] = split(args[0], sp, mr);
        return;
    }
    outs[0] = split(args[0], args[1], mr);
}

void join_reg(Span<const Value> args, size_t, Span<Value> outs,
              CallContext &ctx)
{
    if (args.empty())
        throw Error("join: requires at least 1 argument",
                     0, 0, "join", "", "numkit:join:nargin");
    const Value &delim = (args.size() >= 2) ? args[1] : Value::Empty;
    outs[0] = join(args[0], delim, ctx.engine->resource());
}

} // namespace detail

} // namespace numkit::builtin

// ════════════════════════════════════════════════════════════════════════
// Registration — keeps the existing BuiltinLibrary::registerStringFunctions
// hook alive (now empty); actual wiring happens in library.cpp via
// function-pointer adapters, matching Phase-6c pattern.
// ════════════════════════════════════════════════════════════════════════

namespace numkit {

void BuiltinLibrary::registerStringFunctions(Engine &)
{
    // Intentionally empty — all string builtins now register via the
    // Phase-6c function-pointer path in BuiltinLibrary::install().
}

} // namespace numkit
