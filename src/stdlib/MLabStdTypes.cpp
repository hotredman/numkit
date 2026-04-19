#include "MLabStdLibrary.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <type_traits>

namespace mlab {

// ── Saturating cast from double to integer type T ──────────
template <typename T>
static T saturateCast(double v)
{
    if (std::isnan(v)) return 0;
    v = std::round(v);
    constexpr double lo = static_cast<double>(std::numeric_limits<T>::min());
    constexpr double hi = static_cast<double>(std::numeric_limits<T>::max());
    if (v < lo) return std::numeric_limits<T>::min();
    if (v > hi) return std::numeric_limits<T>::max();
    return static_cast<T>(v);
}

// ── Read element i of any numeric MValue as double ─────────
static double readAsDouble(const MValue &v, size_t i)
{
    switch (v.type()) {
    case MType::DOUBLE:  return v.doubleData()[i];
    case MType::SINGLE:  return static_cast<double>(v.singleData()[i]);
    case MType::LOGICAL: return v.logicalData()[i] ? 1.0 : 0.0;
    case MType::INT8:    return static_cast<double>(v.int8Data()[i]);
    case MType::INT16:   return static_cast<double>(v.int16Data()[i]);
    case MType::INT32:   return static_cast<double>(v.int32Data()[i]);
    case MType::INT64:   return static_cast<double>(v.int64Data()[i]);
    case MType::UINT8:   return static_cast<double>(v.logicalData()[i]); // uint8 = same as logical storage
    case MType::UINT16:  return static_cast<double>(v.uint16Data()[i]);
    case MType::UINT32:  return static_cast<double>(v.uint32Data()[i]);
    case MType::UINT64:  return static_cast<double>(v.uint64Data()[i]);
    case MType::CHAR:    return static_cast<double>(static_cast<unsigned char>(v.charData()[i]));
    default: return 0.0;
    }
}

// ── Generic numeric type constructor: int8(), uint32(), single(), etc. ──
// Converts input to target MType with saturation for integers.
template <typename T>
static void numericConstructor(MType targetType, Span<const MValue> args,
                               Span<MValue> outs, Allocator *alloc)
{
    if (args.empty()) {
        // No args → scalar zero
        auto r = MValue::matrix(1, 1, targetType, alloc);
        *static_cast<T *>(r.rawDataMut()) = static_cast<T>(0);
        outs[0] = std::move(r);
        return;
    }
    auto &a = args[0];
    if (a.type() == targetType) {
        outs[0] = a;
        return;
    }
    size_t n = a.numel();
    MValue r = a.dims().is3D()
                   ? MValue::matrix3d(a.dims().rows(), a.dims().cols(),
                                      a.dims().pages(), targetType, alloc)
                   : MValue::matrix(a.dims().rows(),
                                    std::max(a.dims().cols(), size_t(1)),
                                    targetType, alloc);
    T *dst = static_cast<T *>(r.rawDataMut());
    for (size_t i = 0; i < n; ++i) {
        double v = readAsDouble(a, i);
        if constexpr (std::is_integral_v<T>)
            dst[i] = saturateCast<T>(v);
        else
            dst[i] = static_cast<T>(v);
    }
    outs[0] = std::move(r);
}

// ── isequal helper: compare two MValues deeply ─────────────
static bool valuesEqual(const MValue &a, const MValue &b, bool nanEqual)
{
    if (a.type() != b.type()) return false;
    if (a.dims().rows() != b.dims().rows() || a.dims().cols() != b.dims().cols()) return false;
    if (a.dims().is3D() != b.dims().is3D()) return false;
    if (a.dims().is3D() && a.dims().pages() != b.dims().pages()) return false;

    MType t = a.type();
    size_t n = a.numel();

    if (t == MType::DOUBLE) {
        const double *da = a.doubleData(), *db = b.doubleData();
        for (size_t i = 0; i < n; ++i) {
            if (da[i] == db[i]) continue;
            if (nanEqual && std::isnan(da[i]) && std::isnan(db[i])) continue;
            return false;
        }
        return true;
    }
    if (t == MType::SINGLE) {
        const float *fa = a.singleData(), *fb = b.singleData();
        for (size_t i = 0; i < n; ++i) {
            if (fa[i] == fb[i]) continue;
            if (nanEqual && std::isnan(fa[i]) && std::isnan(fb[i])) continue;
            return false;
        }
        return true;
    }
    if (t == MType::COMPLEX) {
        const Complex *ca = a.complexData(), *cb = b.complexData();
        for (size_t i = 0; i < n; ++i) {
            if (ca[i] == cb[i]) continue;
            if (nanEqual) {
                bool rEq = (ca[i].real() == cb[i].real()) || (std::isnan(ca[i].real()) && std::isnan(cb[i].real()));
                bool iEq = (ca[i].imag() == cb[i].imag()) || (std::isnan(ca[i].imag()) && std::isnan(cb[i].imag()));
                if (rEq && iEq) continue;
            }
            return false;
        }
        return true;
    }
    if (t == MType::CHAR) {
        return std::memcmp(a.charData(), b.charData(), n) == 0;
    }
    if (t == MType::LOGICAL || t == MType::UINT8) {
        return std::memcmp(a.rawData(), b.rawData(), n) == 0;
    }
    if (isIntegerType(t)) {
        return std::memcmp(a.rawData(), b.rawData(), n * elementSize(t)) == 0;
    }
    if (t == MType::CELL) {
        for (size_t i = 0; i < n; ++i)
            if (!valuesEqual(a.cellAt(i), b.cellAt(i), nanEqual)) return false;
        return true;
    }
    if (t == MType::STRUCT) {
        auto &fa = a.structFields(), &fb = b.structFields();
        if (fa.size() != fb.size()) return false;
        for (auto &[k, v] : fa) {
            auto it = fb.find(k);
            if (it == fb.end()) return false;
            if (!valuesEqual(v, it->second, nanEqual)) return false;
        }
        return true;
    }
    if (t == MType::STRING) {
        return a.toString() == b.toString();
    }
    return false;
}

void StdLibrary::registerTypeFunctions(Engine &engine)
{
    engine.registerFunction(
        "double", [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            numericConstructor<double>(MType::DOUBLE, args, outs, &ctx.engine->allocator());
        });

    engine.registerFunction("logical",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                auto &a = args[0];
                                if (a.isLogical()) {
                                    outs[0] = a;
                                    return;
                                }
                                if (a.isScalar()) {
                                    outs[0] = MValue::logicalScalar(a.toScalar() != 0, alloc);
                                    return;
                                }
                                MValue r = a.dims().is3D()
                                               ? MValue::matrix3d(a.dims().rows(),
                                                                  a.dims().cols(),
                                                                  a.dims().pages(),
                                                                  MType::LOGICAL,
                                                                  alloc)
                                               : MValue::matrix(a.dims().rows(),
                                                                a.dims().cols(),
                                                                MType::LOGICAL,
                                                                alloc);
                                for (size_t i = 0; i < a.numel(); ++i)
                                    r.logicalDataMut()[i] = readAsDouble(a, i) != 0 ? 1 : 0;
                                {
                                    outs[0] = r;
                                    return;
                                }
                            });

    engine.registerFunction("char",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                auto &a = args[0];
                                if (a.isChar()) {
                                    outs[0] = a;
                                    return;
                                }
                                if (a.isScalar()) {
                                    char c = static_cast<char>(static_cast<int>(a.toScalar()));
                                    {
                                        outs[0] = MValue::fromString(std::string(1, c), alloc);
                                        return;
                                    }
                                }
                                throw std::runtime_error("Cannot convert to char");
                            });

    engine.registerFunction("isnumeric",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                {
                                    outs[0] = MValue::logicalScalar(args[0].isNumeric(),
                                                                    &ctx.engine->allocator());
                                    return;
                                }
                            });

    engine.registerFunction("islogical",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                {
                                    outs[0] = MValue::logicalScalar(args[0].isLogical(),
                                                                    &ctx.engine->allocator());
                                    return;
                                }
                            });

    engine.registerFunction("ischar",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                {
                                    outs[0] = MValue::logicalScalar(args[0].isChar(),
                                                                    &ctx.engine->allocator());
                                    return;
                                }
                            });

    engine.registerFunction("isstring",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                outs[0] = MValue::logicalScalar(args[0].isString(),
                                                                &ctx.engine->allocator());
                            });

    engine.registerFunction("iscell",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                {
                                    outs[0] = MValue::logicalScalar(args[0].isCell(),
                                                                    &ctx.engine->allocator());
                                    return;
                                }
                            });

    engine.registerFunction("isstruct",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                {
                                    outs[0] = MValue::logicalScalar(args[0].isStruct(),
                                                                    &ctx.engine->allocator());
                                    return;
                                }
                            });

    engine.registerFunction("isempty",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                {
                                    outs[0] = MValue::logicalScalar(args[0].isEmpty(),
                                                                    &ctx.engine->allocator());
                                    return;
                                }
                            });

    engine.registerFunction("isscalar",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                {
                                    outs[0] = MValue::logicalScalar(args[0].isScalar(),
                                                                    &ctx.engine->allocator());
                                    return;
                                }
                            });

    engine.registerFunction("isreal",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                {
                                    outs[0] = MValue::logicalScalar(!args[0].isComplex(),
                                                                    &ctx.engine->allocator());
                                    return;
                                }
                            });

    // ── Numeric type constructors ──────────────────────────────

    engine.registerFunction("single", [](Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx) {
        numericConstructor<float>(MType::SINGLE, args, outs, &ctx.engine->allocator());
    });
    engine.registerFunction("int8", [](Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx) {
        numericConstructor<int8_t>(MType::INT8, args, outs, &ctx.engine->allocator());
    });
    engine.registerFunction("int16", [](Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx) {
        numericConstructor<int16_t>(MType::INT16, args, outs, &ctx.engine->allocator());
    });
    engine.registerFunction("int32", [](Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx) {
        numericConstructor<int32_t>(MType::INT32, args, outs, &ctx.engine->allocator());
    });
    engine.registerFunction("int64", [](Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx) {
        numericConstructor<int64_t>(MType::INT64, args, outs, &ctx.engine->allocator());
    });
    engine.registerFunction("uint8", [](Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx) {
        numericConstructor<uint8_t>(MType::UINT8, args, outs, &ctx.engine->allocator());
    });
    engine.registerFunction("uint16", [](Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx) {
        numericConstructor<uint16_t>(MType::UINT16, args, outs, &ctx.engine->allocator());
    });
    engine.registerFunction("uint32", [](Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx) {
        numericConstructor<uint32_t>(MType::UINT32, args, outs, &ctx.engine->allocator());
    });
    engine.registerFunction("uint64", [](Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx) {
        numericConstructor<uint64_t>(MType::UINT64, args, outs, &ctx.engine->allocator());
    });

    // ── Type query functions ────────────────────────────────────

    engine.registerFunction("isinteger", [](Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx) {
        outs[0] = MValue::logicalScalar(isIntegerType(args[0].type()), &ctx.engine->allocator());
    });
    engine.registerFunction("isfloat", [](Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx) {
        outs[0] = MValue::logicalScalar(isFloatType(args[0].type()), &ctx.engine->allocator());
    });
    engine.registerFunction("issingle", [](Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx) {
        outs[0] = MValue::logicalScalar(args[0].type() == MType::SINGLE, &ctx.engine->allocator());
    });

    // ── isequal / isequaln ──────────────────────────────────────

    engine.registerFunction("isequal", [](Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx) {
        if (args.size() < 2) throw std::runtime_error("isequal requires at least 2 arguments");
        bool eq = true;
        for (size_t i = 1; i < args.size() && eq; ++i)
            eq = valuesEqual(args[0], args[i], false);
        outs[0] = MValue::logicalScalar(eq, &ctx.engine->allocator());
    });
    engine.registerFunction("isequaln", [](Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx) {
        if (args.size() < 2) throw std::runtime_error("isequaln requires at least 2 arguments");
        bool eq = true;
        for (size_t i = 1; i < args.size() && eq; ++i)
            eq = valuesEqual(args[0], args[i], true);
        outs[0] = MValue::logicalScalar(eq, &ctx.engine->allocator());
    });

    // ── class() ─────────────────────────────────────────────────

    engine.registerFunction("class", [](Span<const MValue> args, size_t, Span<MValue> outs, CallContext &ctx) {
        outs[0] = MValue::fromString(mtypeName(args[0].type()), &ctx.engine->allocator());
    });

    engine.registerFunction(
        "isnan", [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            auto &a = args[0];
            if (a.isScalar()) {
                outs[0] = MValue::logicalScalar(std::isnan(a.toScalar()), alloc);
                return;
            }
            auto r = MValue::matrix(a.dims().rows(), a.dims().cols(), MType::LOGICAL, alloc);
            for (size_t i = 0; i < a.numel(); ++i)
                r.logicalDataMut()[i] = std::isnan(a.doubleData()[i]) ? 1 : 0;
            {
                outs[0] = r;
                return;
            }
        });

    engine.registerFunction(
        "isinf", [](Span<const MValue> args, size_t nargout, Span<MValue> outs, CallContext &ctx) {
            auto *alloc = &ctx.engine->allocator();
            auto &a = args[0];
            if (a.isScalar()) {
                outs[0] = MValue::logicalScalar(std::isinf(a.toScalar()), alloc);
                return;
            }
            auto r = MValue::matrix(a.dims().rows(), a.dims().cols(), MType::LOGICAL, alloc);
            for (size_t i = 0; i < a.numel(); ++i)
                r.logicalDataMut()[i] = std::isinf(a.doubleData()[i]) ? 1 : 0;
            {
                outs[0] = r;
                return;
            }
        });
}

} // namespace mlab