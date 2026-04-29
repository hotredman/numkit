// libs/builtin/src/datatypes/strings/scan.cpp
//
// Scan-family builtins (fscanf / sscanf / textscan). Shares SizeSpec /
// parseReadSize / shapeFreadOutput with fileio.cpp via io_helpers.hpp.

#include <numkit/builtin/library.hpp>
#include <numkit/builtin/datatypes/strings/scan.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include "io_helpers.hpp"

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

// ════════════════════════════════════════════════════════════════════════
// scanf-cycle primitives (shared between fscanf and sscanf)
// ════════════════════════════════════════════════════════════════════════

namespace {

// scanfCycle's run summary. The matched values flow back via an
// in/out ScratchVec<double>& parameter (kept on the caller's arena),
// so this struct is just the bookkeeping pair.
struct ScanfOut
{
    size_t count;
    size_t bytesConsumed;
};

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
                     detail::SizeSpec sz, std::pmr::memory_resource *mr)
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

// Common fscanf/sscanf body once the input buffer has been materialised.
// Fills outs[0] with the shaped result and, if requested, outs[1] with
// the count. The caller handles optional outputs beyond that.
void scanfEmit(const std::string &input, const std::string &fmt, detail::SizeSpec sz,
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
        outs[0] = detail::shapeFreadOutput(values.data(), values.size(), sz, mr);
    else if (sz.matrix())
        outs[0] = makeCharMatrix(values.data(), values.size(), sz, mr);
    else
        outs[0] = shapeScanfOutput(values.data(), values.size(), hasNum, mr);
    if (nargout > 1)
        outs[1] = Value::scalar(static_cast<double>(r.count), mr);
}

} // namespace

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

void fscanf(Engine &engine, Span<const Value> args, size_t nargout, Span<Value> outs)
{
    std::pmr::memory_resource *mr = engine.resource();
    if (args.size() < 2 || !args[0].isScalar() || !args[1].isChar())
        throw Error("fscanf: requires (fid, format [, size])");
    int fid = static_cast<int>(args[0].toScalar());
    auto *f = engine.findFile(fid);
    if (!f || !f->forRead)
        throw Error("fscanf: invalid file identifier");

    detail::SizeSpec sz{detail::SizeSpec::Kind::Flat, SIZE_MAX, 0, 0};
    if (args.size() >= 3)
        sz = detail::parseReadSize(args[2], "fscanf");

    std::string input(f->buffer.begin() + f->cursor, f->buffer.end());
    std::string fmt = args[1].toString();

    ScanfOut r{0, 0};
    scanfEmit(input, fmt, sz, nargout, outs, mr, r);
    f->cursor += r.bytesConsumed;

    // Populate ferror on a partial match so scripts can distinguish
    // "input exhausted" from "fewer items matched than requested".
    if (f->cursor >= f->buffer.size())
        f->lastError = "End of file reached.";
    else if (sz.limit != SIZE_MAX && r.count < sz.limit)
        f->lastError = "Matching failure.";
}

void sscanf(std::pmr::memory_resource *mr, Span<const Value> args, size_t nargout, Span<Value> outs)
{
    if (args.size() < 2 || !args[0].isChar() || !args[1].isChar())
        throw Error("sscanf: requires (str, format [, size])");

    detail::SizeSpec sz{detail::SizeSpec::Kind::Flat, SIZE_MAX, 0, 0};
    if (args.size() >= 3)
        sz = detail::parseReadSize(args[2], "sscanf");

    std::string fmt = args[1].toString();
    ScanfOut r{0, 0};
    scanfEmit(args[0].toString(), fmt, sz, nargout, outs, mr, r);

    if (nargout > 2)
        outs[2] = Value::fromString("", mr); // errmsg — always empty for now
    if (nargout > 3)
        outs[3] = Value::scalar(static_cast<double>(r.bytesConsumed + 1), mr);
}

// ════════════════════════════════════════════════════════════════════════
// textscan
//
// Tabular/columnar reading, MATLAB-compatible:
//
//   C = textscan(fid, formatSpec)
//   C = textscan(fid, formatSpec, N)
//   C = textscan(str, formatSpec, …)
//   C = textscan(…, 'Delimiter', d, 'EndOfLine', e, 'HeaderLines', k)
//
// Unlike fscanf, textscan returns a CELL ARRAY with one cell per
// conversion in formatSpec — each cell a column vector of that
// conversion's values. Numeric specs (%d %i %u %f %e %g %x %o)
// produce a column of doubles; %s produces a column cell of
// strings (cellstr). N caps the number of full format cycles.
//
// Supported options:
//   'Delimiter'   char array, or cell array of char arrays. Default
//                 is whitespace (space + tab).
//   'EndOfLine'   char array of characters that terminate a line
//                 (and thus also a token). Default "\n". Always
//                 merged into the effective token-boundary set so
//                 multi-line CSV doesn't glue values across rows.
//   'HeaderLines' integer, number of leading lines to skip.
//
// Not yet: %c, %[set]. Unknown names throw.
// ════════════════════════════════════════════════════════════════════════

namespace {

struct TextscanOptions
{
    explicit TextscanOptions(std::pmr::memory_resource *mr) : treatAsEmpty(mr) {}

    // Empty → whitespace-default mode (runs of space/tab/\f/\v + EOL
    // count as a single field boundary). Non-empty → explicit-delim
    // mode: ONE char in this set is one field boundary, and EOL
    // chars are still separate record boundaries that always
    // collapse.
    std::string delimiters;
    std::string endOfLine = "\n";
    size_t headerLines = 0;
    // Line-comment marker. When non-empty, anything from its first
    // occurrence up to the next EndOfLine char is ignored.
    std::string commentStyle;
    // Tokens whose text equals any entry in this set are coerced to
    // "empty" (numeric → EmptyValue, %s → ''). Vector body lives on
    // the per-call arena; the std::string elements stay on the
    // default heap (typically short enough for SSO anyway).
    ScratchVec<std::string> treatAsEmpty;
    // In explicit-delim mode only: when true, runs of delim chars
    // collapse to one boundary (no empty fields emitted). Default
    // false, matching MATLAB; whitespace-default mode always
    // collapses regardless.
    bool multipleDelimsAsOne = false;
    // Value substituted for empty numeric fields. MATLAB default NaN.
    double emptyValue = std::numeric_limits<double>::quiet_NaN();
};

struct TextscanConv { char spec; bool suppress; int width; };

// Parse formatSpec into an ordered conversion list. Unrecognised
// % codes throw before any scanning happens.
ScratchVec<TextscanConv> parseTextscanFormat(std::pmr::memory_resource *mr,
                                              const std::string &fmt)
{
    ScratchVec<TextscanConv> out(mr);
    for (size_t i = 0; i < fmt.size(); ++i) {
        if (fmt[i] != '%')
            continue;                     // literal chars between specs are ignored
        ++i;
        bool suppress = false;
        if (i < fmt.size() && fmt[i] == '*') { suppress = true; ++i; }
        int width = -1;
        while (i < fmt.size() && std::isdigit(static_cast<unsigned char>(fmt[i]))) {
            if (width < 0) width = 0;
            width = width * 10 + (fmt[i] - '0');
            ++i;
        }
        if (i >= fmt.size())
            throw Error("textscan: truncated format specifier");
        char spec = fmt[i];
        switch (spec) {
        case 'd': case 'i': case 'u':
        case 'f': case 'e': case 'g': case 'E': case 'G':
        case 'x': case 'X': case 'o':
        case 's':
            break;
        default:
            throw Error(std::string("textscan: unsupported conversion '%")
                            + spec + "'");
        }
        out.push_back({spec, suppress, width});
    }
    if (out.empty())
        throw Error("textscan: format must contain at least one conversion");
    return out;
}

} // namespace

void textscan(Engine &engine, Span<const Value> args, size_t nargout, Span<Value> outs)
{
    (void)nargout;
    std::pmr::memory_resource *mr = engine.resource();
    if (args.size() < 2 || !args[1].isChar())
        throw Error("textscan: requires (source, format [, N] [, opt, value …])");

    // Source — fid scalar or char array.
    std::string input;
    Engine::OpenFile *srcFile = nullptr;
    if (args[0].isChar()) {
        input = args[0].toString();
    } else if (args[0].isScalar()) {
        int fid = static_cast<int>(args[0].toScalar());
        srcFile = engine.findFile(fid);
        if (!srcFile || !srcFile->forRead)
            throw Error("textscan: invalid file identifier");
        input.assign(srcFile->buffer.begin() + srcFile->cursor,
                     srcFile->buffer.end());
    } else {
        throw Error("textscan: source must be a file identifier or char array");
    }

    std::string fmt = args[1].toString();
    ScratchArena scratch(mr);
    auto convs = parseTextscanFormat(&scratch, fmt);

    // Optional positional N, then name-value pairs.
    size_t argIdx = 2;
    size_t cycleCap = SIZE_MAX;
    if (argIdx < args.size() && args[argIdx].isScalar() && !args[argIdx].isChar()) {
        double d = args[argIdx].toScalar();
        if (!std::isinf(d)) {
            if (d < 0 || !std::isfinite(d))
                throw Error("textscan: N must be Inf or a non-negative integer");
            cycleCap = static_cast<size_t>(d);
        }
        ++argIdx;
    }

    // Parse name/value options into a TextscanOptions struct.
    TextscanOptions opts{&scratch};
    auto lower = [](std::string s) {
        for (char &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    };
    while (argIdx + 1 < args.size()) {
        if (!args[argIdx].isChar())
            throw Error("textscan: option name must be a char array");
        std::string name = lower(args[argIdx].toString());
        const Value &val = args[argIdx + 1];
        if (name == "delimiter") {
            if (val.isChar()) {
                opts.delimiters = val.toString();
            } else if (val.isCell()) {
                // Concatenate every char in the cells into one delimiter set.
                opts.delimiters.clear();
                for (size_t i = 0; i < val.numel(); ++i) {
                    const Value &d = val.cellAt(i);
                    if (d.isChar()) opts.delimiters += d.toString();
                }
            } else {
                throw Error("textscan: 'Delimiter' must be a char array or cell");
            }
        } else if (name == "endofline") {
            if (!val.isChar())
                throw Error("textscan: 'EndOfLine' must be a char array");
            opts.endOfLine = val.toString();
        } else if (name == "headerlines") {
            double d = val.toScalar();
            if (d < 0 || !std::isfinite(d))
                throw Error("textscan: 'HeaderLines' must be a non-negative integer");
            opts.headerLines = static_cast<size_t>(d);
        } else if (name == "commentstyle") {
            if (!val.isChar())
                throw Error("textscan: 'CommentStyle' must be a char array");
            opts.commentStyle = val.toString();
        } else if (name == "treatasempty") {
            if (val.isChar()) {
                opts.treatAsEmpty.push_back(val.toString());
            } else if (val.isCell()) {
                for (size_t i = 0; i < val.numel(); ++i) {
                    const Value &e = val.cellAt(i);
                    if (e.isChar()) opts.treatAsEmpty.push_back(e.toString());
                }
            } else {
                throw Error("textscan: 'TreatAsEmpty' must be a char array or cell");
            }
        } else if (name == "multipledelimsasone") {
            if (!val.isScalar() && !val.isLogical())
                throw Error("textscan: 'MultipleDelimsAsOne' must be logical/numeric");
            opts.multipleDelimsAsOne = (val.toScalar() != 0.0);
        } else if (name == "emptyvalue") {
            if (!val.isScalar())
                throw Error("textscan: 'EmptyValue' must be a numeric scalar");
            opts.emptyValue = val.toScalar();
        } else {
            throw Error("textscan: unsupported option '" + args[argIdx].toString()
                            + "'");
        }
        argIdx += 2;
    }

    // Character classification. Whitespace-default mode (no
    // Delimiter supplied) treats whitespace as a collapsing
    // field separator; explicit-delim mode reserves whitespace
    // for trimming around fields instead.
    const bool useWsDefault = opts.delimiters.empty();
    size_t pos = 0;

    auto isEol = [&opts](char c) {
        return opts.endOfLine.find(c) != std::string::npos;
    };
    auto isWs = [](char c) {
        return c == ' ' || c == '\t' || c == '\f' || c == '\v';
    };
    auto isExplicitDelim = [&opts](char c) {
        return opts.delimiters.find(c) != std::string::npos;
    };
    auto atComment = [&opts, &input, &pos]() -> bool {
        return !opts.commentStyle.empty() &&
               pos + opts.commentStyle.size() <= input.size() &&
               input.compare(pos, opts.commentStyle.size(), opts.commentStyle) == 0;
    };
    auto isEmpty = [&opts](const std::string &tok) {
        for (auto &e : opts.treatAsEmpty)
            if (tok == e) return true;
        return false;
    };

    // Skip header lines using the configured EndOfLine chars.
    for (size_t h = 0; h < opts.headerLines && pos < input.size(); ++h) {
        while (pos < input.size() && !isEol(input[pos])) ++pos;
        if (pos < input.size()) ++pos;  // consume the EOL char itself
    }

    // skipLeading: eat all whitespace / EOL / comment chars before
    // the very first field of the whole run. Called once, before
    // the cycle loop.
    auto skipLeading = [&]() {
        while (pos < input.size()) {
            if (atComment()) {
                while (pos < input.size() && !isEol(input[pos])) ++pos;
                continue;
            }
            char c = input[pos];
            if (isEol(c) || isWs(c)) { ++pos; continue; }
            break;
        }
    };

    // consumeBoundary: eat exactly ONE field/record boundary
    // between two adjacent fields. Returns false when there is
    // no boundary to consume (EOF or unexpected input), which
    // signals the caller to end the cycle.
    //
    // Whitespace-default: runs of whitespace+EOL+comments all
    // collapse to one boundary — MATLAB's documented contract.
    // Explicit-delim: EOL still collapses, but a single delim
    // char counts as ONE boundary, so "1,,3" yields three
    // fields with the middle one empty (subject to
    // MultipleDelimsAsOne, which collapses runs of the
    // explicit delim back together).
    auto consumeBoundary = [&]() -> bool {
        if (useWsDefault) {
            size_t before = pos;
            while (pos < input.size()) {
                if (atComment()) {
                    while (pos < input.size() && !isEol(input[pos])) ++pos;
                    continue;
                }
                char c = input[pos];
                if (isWs(c) || isEol(c)) { ++pos; continue; }
                break;
            }
            return pos > before;
        }

        // Explicit-delim mode. Trim leading whitespace around
        // the boundary (ws is always stripped from fields, not
        // treated as a delim itself).
        while (pos < input.size() && isWs(input[pos])) ++pos;
        if (atComment()) {
            while (pos < input.size() && !isEol(input[pos])) ++pos;
        }
        if (pos >= input.size()) return false;

        char c = input[pos];
        if (isEol(c)) {
            // EOL runs always collapse, even in explicit-delim mode.
            while (pos < input.size() && isEol(input[pos])) ++pos;
            while (pos < input.size() && isWs(input[pos])) ++pos;
            return true;
        }
        if (!isExplicitDelim(c)) return false;

        ++pos;  // consume exactly one delim
        if (opts.multipleDelimsAsOne) {
            while (pos < input.size() && isExplicitDelim(input[pos])) ++pos;
        }
        while (pos < input.size() && isWs(input[pos])) ++pos;
        return true;
    };

    // readField: consume up to the next field/record boundary.
    // In explicit-delim mode, trailing whitespace inside the
    // field is trimmed (so "1 , 2" with delim ',' yields "1"
    // and "2", not "1 " and " 2").
    auto readField = [&]() -> std::string {
        size_t start = pos;
        while (pos < input.size()) {
            char c = input[pos];
            if (atComment()) break;
            if (isEol(c)) break;
            if (useWsDefault) {
                if (isWs(c)) break;
            } else {
                if (isExplicitDelim(c)) break;
            }
            ++pos;
        }
        size_t end = pos;
        if (!useWsDefault) {
            while (end > start && isWs(input[end - 1])) --end;
        }
        return input.substr(start, end - start);
    };

    // Per-column accumulators. Outer is on the arena; inner pmr-vectors
    // pick up the arena via uses-allocator construction, so every
    // push_back below allocates from the arena.
    ScratchVec<ScratchVec<double>> numCols(convs.size(), &scratch);
    ScratchVec<ScratchVec<std::string>> strCols(convs.size(), &scratch);
    // Staging buffers are HOISTED out of the cycle loop and clear()-ed
    // each iteration: re-creating them inside the loop would bump-leak
    // under the monotonic resource (footgun #c in scratch.hpp).
    // After a few iterations the capacity stabilises and clear() reuses
    // it — total arena footprint stays bounded.
    ScratchVec<std::pair<size_t, double>>      stageNum(&scratch);
    ScratchVec<std::pair<size_t, std::string>> stageStr(&scratch);
    size_t cycles = 0;

    skipLeading();                         // past leading ws/EOL/comments once
    bool atFirstField = true;              // no boundary-consume before the very first field

    while (cycles < cycleCap) {
        if (pos >= input.size()) break;

        size_t savedPos = pos;
        bool savedFirst = atFirstField;
        stageNum.clear();
        stageStr.clear();
        bool fullCycle = true;

        for (size_t i = 0; i < convs.size(); ++i) {
            if (!atFirstField) {
                if (!consumeBoundary()) { fullCycle = false; break; }
            }
            atFirstField = false;

            // Note: we DO NOT bail on pos >= input.size() here.
            // In explicit-delim mode a trailing separator creates
            // a legitimate final empty field ("a,b," → 3 fields),
            // and readField returning "" below gets routed into
            // the empty-field branch correctly. In whitespace-
            // default mode, the same empty-tok path distinguishes
            // "no more data" and triggers fullCycle = false.
            std::string tok = readField();

            // Two kinds of "empty token" can flow out of readField:
            //
            //   1. The raw token was non-empty but matches one of
            //      the TreatAsEmpty markers → coerce to empty,
            //      treat as an explicitly empty field.
            //   2. readField genuinely returned "" because we hit
            //      EOL/EOF → partial cycle in ws-default mode, or
            //      a real empty field between two explicit delims.
            //
            // The coercedEmpty flag keeps these apart so a
            // TreatAsEmpty hit in whitespace-default mode still
            // yields EmptyValue instead of rolling back.
            bool coercedEmpty = !tok.empty() && isEmpty(tok);
            if (coercedEmpty) tok.clear();

            const TextscanConv &c = convs[i];
            if (tok.empty()) {
                if (!coercedEmpty && useWsDefault) {
                    fullCycle = false;
                    break;
                }
                if (c.suppress) continue;
                if (c.spec == 's')
                    stageStr.push_back({i, std::string()});
                else
                    stageNum.push_back({i, opts.emptyValue});
                continue;
            }

            if (c.width > 0 && static_cast<int>(tok.size()) > c.width)
                tok.resize(static_cast<size_t>(c.width));

            if (c.spec == 's') {
                if (!c.suppress) stageStr.push_back({i, std::move(tok)});
                continue;
            }

            const char *start = tok.c_str();
            char *endp = nullptr;
            double v = 0.0;
            switch (c.spec) {
            case 'd': case 'i': {
                long long iv = std::strtoll(start, &endp, 10);
                v = static_cast<double>(iv);
                break;
            }
            case 'u': {
                unsigned long long uv = std::strtoull(start, &endp, 10);
                v = static_cast<double>(uv);
                break;
            }
            case 'f': case 'e': case 'g': case 'E': case 'G':
                v = std::strtod(start, &endp); break;
            case 'x': case 'X': {
                unsigned long long uv = std::strtoull(start, &endp, 16);
                v = static_cast<double>(uv);
                break;
            }
            case 'o': {
                unsigned long long uv = std::strtoull(start, &endp, 8);
                v = static_cast<double>(uv);
                break;
            }
            }
            if (endp == start) { fullCycle = false; break; }
            if (!c.suppress) stageNum.push_back({i, v});
        }

        if (!fullCycle) {
            pos = savedPos;
            atFirstField = savedFirst;
            break;
        }

        for (auto &p : stageNum) numCols[p.first].push_back(p.second);
        for (auto &p : stageStr) strCols[p.first].push_back(std::move(p.second));
        ++cycles;
    }

    if (srcFile)
        srcFile->cursor += pos;

    // Build the outer cell array — one slot per non-suppressed
    // conversion. Suppressed conversions contribute no column.
    size_t nonSup = 0;
    for (auto &c : convs) if (!c.suppress) ++nonSup;
    Value result = Value::cell(1, nonSup);
    size_t slot = 0;
    for (size_t i = 0; i < convs.size(); ++i) {
        if (convs[i].suppress) continue;
        if (convs[i].spec == 's') {
            size_t k = strCols[i].size();
            Value inner = Value::cell(k, 1);
            for (size_t j = 0; j < k; ++j)
                inner.cellAt(j) = Value::fromString(strCols[i][j], mr);
            result.cellAt(slot++) = std::move(inner);
        } else {
            size_t k = numCols[i].size();
            Value col = (k == 0)
                             ? Value::matrix(0, 0, ValueType::DOUBLE, mr)
                             : Value::matrix(k, 1, ValueType::DOUBLE, mr);
            if (k > 0)
                std::memcpy(col.doubleDataMut(), numCols[i].data(),
                            k * sizeof(double));
            result.cellAt(slot++) = std::move(col);
        }
    }
    outs[0] = std::move(result);
}

// ════════════════════════════════════════════════════════════════════════
// Adapters
// ════════════════════════════════════════════════════════════════════════

namespace detail {

void fscanf_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    fscanf(*ctx.engine, args, nargout, outs);
}

void sscanf_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    sscanf(ctx.engine->resource(), args, nargout, outs);
}

void textscan_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    textscan(*ctx.engine, args, nargout, outs);
}

} // namespace detail

} // namespace numkit::builtin
