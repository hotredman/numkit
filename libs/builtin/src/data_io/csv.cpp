// libs/builtin/src/data_io/csv.cpp
//
// CSV text I/O (csvread / csvwrite), routed through Engine's Vfs.

#include <numkit/builtin/data_io/csv.hpp>
#include <numkit/builtin/library.hpp>

#include <numkit/core/engine.hpp>
#include <numkit/core/scratch.hpp>
#include <numkit/core/types.hpp>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <memory_resource>
#include <sstream>
#include <string>

namespace numkit::builtin {

// ════════════════════════════════════════════════════════════════════════
// Private CSV helpers
// ════════════════════════════════════════════════════════════════════════

namespace {

// Auto-append ".csv" when the filename's basename has no extension.
// Matches MATLAB's quiet behaviour so short names like "out" still DTRT.
std::string resolveCsvPath(std::string path)
{
    size_t slash = path.find_last_of("/\\");
    size_t fromIdx = (slash == std::string::npos) ? 0 : slash + 1;
    if (path.find('.', fromIdx) == std::string::npos)
        path += ".csv";
    return path;
}

// Parse one CSV line into a vector of doubles. Empty / non-numeric
// cells map to 0.0 (matching MATLAB's csvread contract). Trailing \r
// is stripped so CRLF files don't leak a carriage return into the last
// cell's parse range.
ScratchVec<double> parseCsvLine(std::pmr::memory_resource *mr, const std::string &raw)
{
    ScratchVec<double> out(mr);
    size_t len = raw.size();
    if (len && raw[len - 1] == '\r')
        --len;

    size_t start = 0;
    for (size_t i = 0; i <= len; ++i) {
        if (i == len || raw[i] == ',') {
            const char *p = raw.c_str() + start;
            size_t n = i - start;
            while (n > 0 && (*p == ' ' || *p == '\t')) {
                ++p;
                --n;
            }
            while (n > 0 && (p[n - 1] == ' ' || p[n - 1] == '\t'))
                --n;
            double v = 0.0;
            if (n > 0) {
                std::string cell(p, n);
                char *endp = nullptr;
                double parsed = std::strtod(cell.c_str(), &endp);
                if (endp != cell.c_str())
                    v = parsed;
            }
            out.push_back(v);
            start = i + 1;
        }
    }
    return out;
}

} // namespace

// ════════════════════════════════════════════════════════════════════════
// csvread / csvwrite — CSV text I/O via the Engine's Vfs
//
//   csvread(filename)                         → full numeric CSV → matrix
//   csvread(filename, R1, C1)                 → skip first R1 rows / C1 cols
//   csvread(filename, R1, C1, [R1 C1 R2 C2])  → read a rectangular range
//
//   csvwrite(filename, M)                     → write M as CSV
//   csvwrite(filename, M, R, C)               → write with row/col offsets
//
// Missing cells are read as 0. Filenames with no extension get ".csv".
// ════════════════════════════════════════════════════════════════════════

Value csvread(Engine &engine, Span<const Value> args)
{
    std::pmr::memory_resource *mr = engine.resource();
    if (args.empty() || !args[0].isChar())
        throw Error("csvread requires a filename as the first argument");

    std::string filename = resolveCsvPath(args[0].toString());

    size_t r1 = 0, c1 = 0, r2 = 0, c2 = 0;
    bool haveRange = false;

    if (args.size() >= 2)
        r1 = static_cast<size_t>(args[1].toScalar());
    if (args.size() >= 3)
        c1 = static_cast<size_t>(args[2].toScalar());
    if (args.size() >= 4) {
        if (args[3].numel() != 4)
            throw Error("csvread: range argument must be [R1 C1 R2 C2]");
        haveRange = true;
        r2 = static_cast<size_t>(args[3](2));
        c2 = static_cast<size_t>(args[3](3));
        if (r2 < r1 || c2 < c1)
            throw Error("csvread: invalid range [R1 C1 R2 C2]");
    }

    auto resolved = engine.resolvePath(filename);
    std::string content;
    try {
        content = resolved.fs->readFile(resolved.path);
    } catch (const std::exception &e) {
        throw Error(std::string("csvread: ") + e.what());
    }

    ScratchArena scratch(mr);
    // Outer + inner ScratchVec — uses-allocator construction propagates
    // the arena into each inner row that we move-construct from
    // `parseCsvLine`'s sliced result below.
    ScratchVec<ScratchVec<double>> rows(&scratch);
    size_t lineNo = 0;
    size_t pos = 0;
    while (pos <= content.size()) {
        size_t nl = content.find('\n', pos);
        size_t end = (nl == std::string::npos) ? content.size() : nl;
        // Stop cleanly on a trailing newline with nothing after it —
        // otherwise we'd produce one bogus empty row.
        if (end == pos && nl == std::string::npos)
            break;
        std::string line = content.substr(pos, end - pos);
        pos = (nl == std::string::npos) ? content.size() + 1 : nl + 1;

        if (lineNo < r1) {
            ++lineNo;
            continue;
        }
        if (haveRange && lineNo > r2)
            break;
        ++lineNo;

        ScratchVec<double> cells = parseCsvLine(&scratch, line);
        ScratchVec<double> row(&scratch);
        size_t endCol = haveRange ? std::min(c2 + 1, cells.size()) : cells.size();
        if (c1 < endCol)
            row.assign(cells.begin() + c1, cells.begin() + endCol);
        rows.push_back(std::move(row));
    }

    size_t R, C;
    if (haveRange) {
        R = r2 - r1 + 1;
        C = c2 - c1 + 1;
        while (rows.size() < R)
            rows.emplace_back();   // mr propagates via uses-allocator ctor
    } else {
        R = rows.size();
        C = 0;
        for (auto &r : rows)
            if (r.size() > C)
                C = r.size();
    }

    if (R == 0 || C == 0)
        return Value::matrix(0, 0, ValueType::DOUBLE, mr);

    auto M = Value::matrix(R, C, ValueType::DOUBLE, mr);
    double *data = M.doubleDataMut();
    for (size_t r = 0; r < R; ++r) {
        const auto &row = rows[r];
        size_t n = std::min(C, row.size());
        for (size_t c = 0; c < n; ++c)
            data[c * R + r] = row[c];
    }
    return M;
}

void csvwrite(Engine &engine, Span<const Value> args)
{
    if (args.size() < 2)
        throw Error("csvwrite requires filename and matrix arguments");
    if (!args[0].isChar())
        throw Error("csvwrite: first argument must be a filename");

    std::string filename = resolveCsvPath(args[0].toString());
    const Value &M = args[1];

    size_t offR = 0, offC = 0;
    if (args.size() >= 3)
        offR = static_cast<size_t>(args[2].toScalar());
    if (args.size() >= 4)
        offC = static_cast<size_t>(args[3].toScalar());

    size_t R = M.dims().rows();
    size_t C = M.dims().cols();

    std::ostringstream os;

    for (size_t r = 0; r < offR; ++r)
        os << '\n';

    auto writeCell = [&](double v) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.15g", v);
        os << buf;
    };

    for (size_t r = 0; r < R; ++r) {
        for (size_t c = 0; c < offC; ++c)
            os << ',';
        for (size_t c = 0; c < C; ++c) {
            if (c > 0)
                os << ',';
            double v = 0.0;
            if (M.type() == ValueType::DOUBLE) {
                v = M.isScalar() ? M.toScalar() : M(r, c);
            } else if (M.isLogical()) {
                if (M.isScalar())
                    v = M.toBool() ? 1.0 : 0.0;
                else
                    v = M.logicalData()[M.dims().sub2ind(r, c)] ? 1.0 : 0.0;
            } else if (M.isComplex()) {
                v = M.isScalar() ? M.toComplex().real() : M.complexElem(r, c).real();
            }
            writeCell(v);
        }
        os << '\n';
    }

    auto resolved = engine.resolvePath(filename);
    try {
        resolved.fs->writeFile(resolved.path, os.str());
    } catch (const std::exception &e) {
        throw Error(std::string("csvwrite: ") + e.what());
    }
}

// ════════════════════════════════════════════════════════════════════════
// Adapters
// ════════════════════════════════════════════════════════════════════════

namespace detail {

void csvread_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    (void)nargout;
    outs[0] = csvread(*ctx.engine, args);
}

void csvwrite_reg(Span<const Value> args, size_t nargout, Span<Value> outs, CallContext &ctx)
{
    (void)nargout;
    (void)outs;
    csvwrite(*ctx.engine, args);
}

} // namespace detail

} // namespace numkit::builtin
