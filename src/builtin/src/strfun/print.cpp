// toolboxes/builtin/src/datatypes/strings/print.cpp

#include <numkit/builtin/strfun.hpp>
#include <numkit/builtin/strfun.hpp>

#include <numkit/value/value.hpp>
#include <numkit/value/shape_ops.hpp>
#include <numkit/value/error.hpp>

#include <cmath>
#include <cstring>
#include <cstdint>
#include <sstream>

namespace numkit::builtin {

// ════════════════════════════════════════════════════════════════════════
// Public API
// ════════════════════════════════════════════════════════════════════════

std::string dispFormat(const Value &a)
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
    } else if (a.type() == ValueType::DOUBLE) {
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

} // namespace numkit::builtin
