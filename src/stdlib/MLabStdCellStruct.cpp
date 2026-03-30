#include "MLabStdLibrary.hpp"

namespace mlab {

void StdLibrary::registerCellStructFunctions(Engine &engine)
{
    engine.registerFunction("struct", [](const std::vector<MValue> &args) -> std::vector<MValue> {
        auto s = MValue::structure();
        for (size_t i = 0; i + 1 < args.size(); i += 2)
            s.field(args[i].toString()) = args[i + 1];
        return {s};
    });

    engine.registerFunction("fieldnames",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                auto &a = args[0];
                                if (!a.isStruct())
                                    throw std::runtime_error("fieldnames requires a struct");
                                auto &fields = a.structFields();
                                auto c = MValue::cell(fields.size(), 1);
                                size_t i = 0;
                                for (auto &[k, v] : fields)
                                    c.cellAt(i++) = MValue::fromString(k, alloc);
                                return {c};
                            });

    engine.registerFunction("isfield",
                            [&engine](const std::vector<MValue> &args) -> std::vector<MValue> {
                                auto *alloc = &engine.allocator();
                                if (!args[0].isStruct())
                                    return {MValue::logicalScalar(false, alloc)};
                                return {MValue::logicalScalar(args[0].hasField(args[1].toString()), alloc)};
                            });

    engine.registerFunction("rmfield", [](const std::vector<MValue> &args) -> std::vector<MValue> {
        auto s = args[0];
        if (!s.isStruct())
            throw std::runtime_error("rmfield requires a struct");
        s.structFields().erase(args[1].toString());
        return {s};
    });

    engine.registerFunction("cell", [](const std::vector<MValue> &args) -> std::vector<MValue> {
        size_t r = static_cast<size_t>(args[0].toScalar());
        size_t c = args.size() >= 2 ? static_cast<size_t>(args[1].toScalar()) : r;
        return {MValue::cell(r, c)};
    });
}

} // namespace mlab
