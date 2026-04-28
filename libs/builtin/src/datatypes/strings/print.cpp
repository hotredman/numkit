// libs/builtin/src/MStdPrint.cpp

#include <numkit/m/builtin/datatypes/strings/format.hpp>
#include <numkit/m/builtin/MStdLibrary.hpp>
#include <numkit/m/builtin/datatypes/strings/print.hpp>

#include <numkit/m/core/MEngine.hpp>
#include <numkit/m/core/MShapeOps.hpp>
#include <numkit/m/core/MTypes.hpp>

#include <cmath>
#include <cstring>
#include <cstdint>
#include <sstream>

namespace numkit::m::builtin {

// ════════════════════════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════════════════════════

std::string dispFormat(const MValue &a)
{
    std::ostringstream os;
    if (a.isChar()) {
        // Row vector (or 1×0): single line. Matrix: one row per line.
        auto d = a.dims();
        if (d.rows() <= 1) {
            os << a.toString();
        } else {
            for (size_t r = 0; r < d.rows(); ++r) {
                if (r > 0) os << "\n";
                os << a.charRow(r);
            }
        }
    } else if (a.isEmpty()) {
        os << "[]";
    } else if (a.type() == MType::DOUBLE) {
        if (a.isScalar()) {
            os << a.toScalar();
        } else {
            auto d = a.dims();
            if (d.rows() == 1) {
                os << "[";
                for (size_t c = 0; c < d.cols(); ++c) {
                    if (c > 0) os << " ";
                    double v = a(0, c);
                    if (v == std::floor(v) && std::isfinite(v))
                        os << static_cast<long long>(v);
                    else
                        os << v;
                }
                os << "]";
            } else if (d.cols() == 1) {
                for (size_t r = 0; r < d.rows(); ++r) {
                    if (r > 0) os << "\n";
                    double v = a(r, 0);
                    if (v == std::floor(v) && std::isfinite(v))
                        os << "   " << static_cast<long long>(v);
                    else
                        os << "   " << v;
                }
            } else {
                const size_t R = d.rows(), C = d.cols();
                const int nd = d.ndim();
                const double *base0 = a.doubleData();
                forEachOuterPage(d, [&](size_t plin, const size_t *outerCoords) {
                    if (nd >= 3) {
                        os << "(:,:";
                        for (int i = 2; i < nd; ++i)
                            os << "," << outerCoords[i - 2] + 1;
                        os << ") =\n";
                    }
                    const double *page = base0 + plin * R * C;
                    for (size_t r = 0; r < R; ++r) {
                        if (r > 0) os << "\n";
                        os << "   ";
                        for (size_t c = 0; c < C; ++c) {
                            double v = page[c * R + r];
                            if (v == std::floor(v) && std::isfinite(v))
                                os << " " << static_cast<long long>(v);
                            else
                                os << " " << v;
                        }
                    }
                });
            }
        }
    } else if (a.isLogical()) {
        if (a.isScalar()) {
            os << (a.toBool() ? "1" : "0");
        } else {
            auto d = a.dims();
            const uint8_t *ld = a.logicalData();
            for (size_t r = 0; r < d.rows(); ++r) {
                if (r > 0) os << "\n";
                os << "   ";
                for (size_t c = 0; c < d.cols(); ++c)
                    os << " " << (ld[d.sub2ind(r, c)] ? "1" : "0");
            }
        }
    } else if (a.isStruct()) {
        for (auto &[k, v] : a.structFields())
            os << "    " << k << ": " << v.debugString() << "\n";
    } else if (a.isCell()) {
        auto d = a.dims();
        os << "{" << d.rows() << "x" << d.cols() << " cell}";
    } else if (a.isComplex()) {
        if (a.isScalar()) {
            auto c = a.toComplex();
            if (c.real() != 0.0 || c.imag() == 0.0)
                os << c.real();
            if (c.imag() != 0.0) {
                if (c.real() != 0.0 && c.imag() > 0)
                    os << "+";
                os << c.imag() << "i";
            }
        } else {
            os << a.debugString();
        }
    } else {
        os << a.debugString();
    }
    os << "\n";
    return os.str();
}

void disp(Engine &engine, Span<const MValue> args)
{
    for (const auto &a : args)
        engine.outputText(dispFormat(a));
}

void fprintf(Engine &engine, Span<const MValue> args)
{
    if (args.empty())
        return;
    Allocator &alloc = engine.allocator();

    // First-arg-is-fid disambiguation: MATLAB allows both
    //   fprintf(format, …)  and  fprintf(fid, format, …)
    // We detect the latter via "leading numeric scalar followed by char".
    int fid = 1;
    size_t fmtIdx = 0;
    if (args.size() >= 2 && args[0].isScalar() && args[1].isChar()) {
        fid = static_cast<int>(args[0].toScalar());
        fmtIdx = 1;
    }
    if (!args[fmtIdx].isChar())
        return;

    std::string result = formatCyclic(alloc, args[fmtIdx].toString(), args, fmtIdx + 1);

    if (fid == 1 || fid == 2) {
        engine.outputText(result);
    } else if (fid >= 3) {
        auto *f = engine.findFile(fid);
        if (!f || !f->forWrite)
            throw MError("fprintf: invalid file identifier");
        // For 'a'/'a+' (appendOnly) snap to end first — MATLAB's
        // contract regardless of prior seek.
        size_t writePos = f->appendOnly ? f->buffer.size() : f->cursor;
        if (writePos + result.size() > f->buffer.size())
            f->buffer.resize(writePos + result.size());
        std::memcpy(f->buffer.data() + writePos, result.data(), result.size());
        f->cursor = writePos + result.size();
    } else {
        throw MError("fprintf: invalid file identifier");
    }
}

// ════════════════════════════════════════════════════════════════════════
// Adapters
// ════════════════════════════════════════════════════════════════════════

namespace detail {

void disp_reg(Span<const MValue> args, size_t, Span<MValue>, CallContext &ctx)
{
    disp(*ctx.engine, args);
}

void fprintf_reg(Span<const MValue> args, size_t, Span<MValue>, CallContext &ctx)
{
    fprintf(*ctx.engine, args);
}

} // namespace detail

} // namespace numkit::m::builtin
