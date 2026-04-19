#include "MStdLibrary.hpp"

namespace numkit {

void StdLibrary::registerCellStructFunctions(Engine &engine)
{
    engine.registerFunction("struct",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto s = MValue::structure();
                                for (size_t i = 0; i + 1 < args.size(); i += 2)
                                    s.field(args[i].toString()) = args[i + 1];
                                {
                                    outs[0] = s;
                                    return;
                                }
                            });

    engine.registerFunction("fieldnames",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                auto &a = args[0];
                                if (!a.isStruct())
                                    throw std::runtime_error("fieldnames requires a struct");
                                auto &fields = a.structFields();
                                auto c = MValue::cell(fields.size(), 1);
                                size_t i = 0;
                                for (auto &[k, v] : fields)
                                    c.cellAt(i++) = MValue::fromString(k, alloc);
                                {
                                    outs[0] = c;
                                    return;
                                }
                            });

    engine.registerFunction("isfield",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto *alloc = &ctx.engine->allocator();
                                if (!args[0].isStruct()) {
                                    outs[0] = MValue::logicalScalar(false, alloc);
                                    return;
                                }
                                {
                                    outs[0] = MValue::logicalScalar(args[0].hasField(
                                                                        args[1].toString()),
                                                                    alloc);
                                    return;
                                }
                            });

    engine.registerFunction("rmfield",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                auto s = args[0];
                                if (!s.isStruct())
                                    throw std::runtime_error("rmfield requires a struct");
                                s.structFields().erase(args[1].toString());
                                {
                                    outs[0] = s;
                                    return;
                                }
                            });

    engine.registerFunction("cell",
                            [](Span<const MValue> args,
                               size_t nargout,
                               Span<MValue> outs,
                               CallContext &ctx) {
                                size_t r = static_cast<size_t>(args[0].toScalar());
                                size_t c = args.size() >= 2
                                               ? static_cast<size_t>(args[1].toScalar())
                                               : r;
                                if (args.size() >= 3) {
                                    size_t p = static_cast<size_t>(args[2].toScalar());
                                    outs[0] = MValue::cell3D(r, c, p);
                                } else {
                                    outs[0] = MValue::cell(r, c);
                                }
                                return;
                            });
}

} // namespace numkit