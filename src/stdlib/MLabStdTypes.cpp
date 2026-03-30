#include "MLabStdLibrary.hpp"

#include <cmath>

namespace mlab {

void StdLibrary::registerTypeFunctions(Engine &engine)
{
    engine.registerFunction("double",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                auto &a = args[0];
                                if (a.type() == MType::DOUBLE) return {a};
                                if (a.isLogical()) return {MValue::scalar(a.toBool() ? 1.0 : 0.0, alloc)};
                                if (a.isChar())
                                    return {MValue::scalar(static_cast<double>(
                                        static_cast<unsigned char>(a.charData()[0])), alloc)};
                                throw std::runtime_error("Cannot convert to double");
                            });

    engine.registerFunction("logical",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                auto &a = args[0];
                                if (a.isLogical()) return {a};
                                if (a.isScalar()) return {MValue::logicalScalar(a.toScalar() != 0, alloc)};
                                auto r = MValue::matrix(a.dims().rows(), a.dims().cols(), MType::LOGICAL, alloc);
                                for (size_t i = 0; i < a.numel(); ++i)
                                    r.logicalDataMut()[i] = a.doubleData()[i] != 0 ? 1 : 0;
                                return {r};
                            });

    engine.registerFunction("char",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                auto &a = args[0];
                                if (a.isChar()) return {a};
                                if (a.isScalar()) {
                                    char c = static_cast<char>(static_cast<int>(a.toScalar()));
                                    return {MValue::fromString(std::string(1, c), alloc)};
                                }
                                throw std::runtime_error("Cannot convert to char");
                            });

    engine.registerFunction("isnumeric",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                return {MValue::logicalScalar(args[0].isNumeric(), &engine.allocator())};
                            });

    engine.registerFunction("islogical",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                return {MValue::logicalScalar(args[0].isLogical(), &engine.allocator())};
                            });

    engine.registerFunction("ischar",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                return {MValue::logicalScalar(args[0].isChar(), &engine.allocator())};
                            });

    engine.registerFunction("iscell",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                return {MValue::logicalScalar(args[0].isCell(), &engine.allocator())};
                            });

    engine.registerFunction("isstruct",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                return {MValue::logicalScalar(args[0].isStruct(), &engine.allocator())};
                            });

    engine.registerFunction("isempty",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                return {MValue::logicalScalar(args[0].isEmpty(), &engine.allocator())};
                            });

    engine.registerFunction("isscalar",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                return {MValue::logicalScalar(args[0].isScalar(), &engine.allocator())};
                            });

    engine.registerFunction("isreal",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                return {MValue::logicalScalar(!args[0].isComplex(), &engine.allocator())};
                            });

    engine.registerFunction("isnan",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                auto &a = args[0];
                                if (a.isScalar())
                                    return {MValue::logicalScalar(std::isnan(a.toScalar()), alloc)};
                                auto r = MValue::matrix(a.dims().rows(), a.dims().cols(), MType::LOGICAL, alloc);
                                for (size_t i = 0; i < a.numel(); ++i)
                                    r.logicalDataMut()[i] = std::isnan(a.doubleData()[i]) ? 1 : 0;
                                return {r};
                            });

    engine.registerFunction("isinf",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                auto &a = args[0];
                                if (a.isScalar())
                                    return {MValue::logicalScalar(std::isinf(a.toScalar()), alloc)};
                                auto r = MValue::matrix(a.dims().rows(), a.dims().cols(), MType::LOGICAL, alloc);
                                for (size_t i = 0; i < a.numel(); ++i)
                                    r.logicalDataMut()[i] = std::isinf(a.doubleData()[i]) ? 1 : 0;
                                return {r};
                            });
}

} // namespace mlab
