#include "MLabStdLibrary.hpp"

#include <cmath>

namespace mlab {

void StdLibrary::registerTypeFunctions(Engine &engine)
{
    engine.registerFunction("double",
                            [&engine](Span<const MValue> args, size_t nargout, Span<MValue> outs) {
                                auto *alloc = &engine.allocator();
                                auto &a = args[0];
                                if (a.type() == MType::DOUBLE) { outs[0] = a; return; }
                                if (a.isLogical()) { outs[0] = MValue::scalar(a.toBool() ? 1.0 : 0.0, alloc); return; }
                                if (a.isChar())
                                    { outs[0] = MValue::scalar(static_cast<double>(
                                        static_cast<unsigned char>(a.charData()[0])), alloc); return; }
                                throw std::runtime_error("Cannot convert to double");
                            });

    engine.registerFunction("logical",
                            [&engine](Span<const MValue> args, size_t nargout, Span<MValue> outs) {
                                auto *alloc = &engine.allocator();
                                auto &a = args[0];
                                if (a.isLogical()) { outs[0] = a; return; }
                                if (a.isScalar()) { outs[0] = MValue::logicalScalar(a.toScalar() != 0, alloc); return; }
                                auto r = MValue::matrix(a.dims().rows(), a.dims().cols(), MType::LOGICAL, alloc);
                                for (size_t i = 0; i < a.numel(); ++i)
                                    r.logicalDataMut()[i] = a.doubleData()[i] != 0 ? 1 : 0;
                                { outs[0] = r; return; }
                            });

    engine.registerFunction("char",
                            [&engine](Span<const MValue> args, size_t nargout, Span<MValue> outs) {
                                auto *alloc = &engine.allocator();
                                auto &a = args[0];
                                if (a.isChar()) { outs[0] = a; return; }
                                if (a.isScalar()) {
                                    char c = static_cast<char>(static_cast<int>(a.toScalar()));
                                    { outs[0] = MValue::fromString(std::string(1, c), alloc); return; }
                                }
                                throw std::runtime_error("Cannot convert to char");
                            });

    engine.registerFunction("isnumeric",
                            [&engine](Span<const MValue> args, size_t nargout, Span<MValue> outs) {
                                { outs[0] = MValue::logicalScalar(args[0].isNumeric(), &engine.allocator()); return; }
                            });

    engine.registerFunction("islogical",
                            [&engine](Span<const MValue> args, size_t nargout, Span<MValue> outs) {
                                { outs[0] = MValue::logicalScalar(args[0].isLogical(), &engine.allocator()); return; }
                            });

    engine.registerFunction("ischar",
                            [&engine](Span<const MValue> args, size_t nargout, Span<MValue> outs) {
                                { outs[0] = MValue::logicalScalar(args[0].isChar(), &engine.allocator()); return; }
                            });

    engine.registerFunction("iscell",
                            [&engine](Span<const MValue> args, size_t nargout, Span<MValue> outs) {
                                { outs[0] = MValue::logicalScalar(args[0].isCell(), &engine.allocator()); return; }
                            });

    engine.registerFunction("isstruct",
                            [&engine](Span<const MValue> args, size_t nargout, Span<MValue> outs) {
                                { outs[0] = MValue::logicalScalar(args[0].isStruct(), &engine.allocator()); return; }
                            });

    engine.registerFunction("isempty",
                            [&engine](Span<const MValue> args, size_t nargout, Span<MValue> outs) {
                                { outs[0] = MValue::logicalScalar(args[0].isEmpty(), &engine.allocator()); return; }
                            });

    engine.registerFunction("isscalar",
                            [&engine](Span<const MValue> args, size_t nargout, Span<MValue> outs) {
                                { outs[0] = MValue::logicalScalar(args[0].isScalar(), &engine.allocator()); return; }
                            });

    engine.registerFunction("isreal",
                            [&engine](Span<const MValue> args, size_t nargout, Span<MValue> outs) {
                                { outs[0] = MValue::logicalScalar(!args[0].isComplex(), &engine.allocator()); return; }
                            });

    engine.registerFunction("isnan",
                            [&engine](Span<const MValue> args, size_t nargout, Span<MValue> outs) {
                                auto *alloc = &engine.allocator();
                                auto &a = args[0];
                                if (a.isScalar())
                                    { outs[0] = MValue::logicalScalar(std::isnan(a.toScalar()), alloc); return; }
                                auto r = MValue::matrix(a.dims().rows(), a.dims().cols(), MType::LOGICAL, alloc);
                                for (size_t i = 0; i < a.numel(); ++i)
                                    r.logicalDataMut()[i] = std::isnan(a.doubleData()[i]) ? 1 : 0;
                                { outs[0] = r; return; }
                            });

    engine.registerFunction("isinf",
                            [&engine](Span<const MValue> args, size_t nargout, Span<MValue> outs) {
                                auto *alloc = &engine.allocator();
                                auto &a = args[0];
                                if (a.isScalar())
                                    { outs[0] = MValue::logicalScalar(std::isinf(a.toScalar()), alloc); return; }
                                auto r = MValue::matrix(a.dims().rows(), a.dims().cols(), MType::LOGICAL, alloc);
                                for (size_t i = 0; i < a.numel(); ++i)
                                    r.logicalDataMut()[i] = std::isinf(a.doubleData()[i]) ? 1 : 0;
                                { outs[0] = r; return; }
                            });
}

} // namespace mlab
