#include "MLabStdLibrary.hpp"

#include <cmath>
#include <iostream>
#include <sstream>

namespace mlab {

void StdLibrary::registerIOFunctions(Engine &engine)
{
    engine.registerFunction("disp",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                for (auto &a : args) {
                                    std::ostringstream os;
                                    if (a.isChar()) {
                                        os << a.toString();
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
                                                for (size_t p = 0; p < d.pages(); ++p) {
                                                    if (d.is3D()) os << "(:,:," << p + 1 << ") =\n";
                                                    for (size_t r = 0; r < d.rows(); ++r) {
                                                        if (r > 0) os << "\n";
                                                        os << "   ";
                                                        for (size_t c = 0; c < d.cols(); ++c) {
                                                            double v = a(r, c, p);
                                                            if (v == std::floor(v) && std::isfinite(v))
                                                                os << " " << static_cast<long long>(v);
                                                            else
                                                                os << " " << v;
                                                        }
                                                    }
                                                }
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
                                                if (c.real() != 0.0 && c.imag() > 0) os << "+";
                                                os << c.imag() << "i";
                                            }
                                        } else {
                                            os << a.debugString();
                                        }
                                    } else {
                                        os << a.debugString();
                                    }
                                    os << "\n";
                                    engine.outputText(os.str());
                                }
                                return {};
                            });

    engine.registerFunction("fprintf",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                if (!args.empty() && args[0].isChar())
                                    engine.outputText(args[0].toString());
                                return {};
                            });

    engine.registerFunction("sprintf",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                if (!args.empty() && args[0].isChar())
                                    return {MValue::fromString(args[0].toString(), alloc)};
                                return {MValue::fromString("", alloc)};
                            });

    engine.registerFunction("error", [](const std::vector<MValue> &args) -> std::vector<MValue> {
        std::string msg = args.empty() ? "Error" : args[0].toString();
        throw std::runtime_error(msg);
    });

    engine.registerFunction("warning", [](const std::vector<MValue> &args) -> std::vector<MValue> {
        if (!args.empty() && args[0].isChar())
            std::cerr << "Warning: " << args[0].toString() << "\n";
        return {};
    });
}

} // namespace mlab
