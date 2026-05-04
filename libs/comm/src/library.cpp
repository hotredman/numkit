// libs/comm/src/library.cpp

#include <numkit/comm/library.hpp>

#include <numkit/core/types.hpp>

namespace numkit::comm::detail {
// modulation/psk.cpp
void pskmod_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void pskdemod_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void dpskmod_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void dpskdemod_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
} // namespace numkit::comm::detail

namespace numkit {

void CommLibrary::install(Engine &engine)
{
    auto reg = [&](const char *sub, const char *name, ExternalFunc fn) {
        engine.registerFunction(std::string("comm.") + sub, name, fn);
        engine.registerFunction("compat", name, fn);
    };

    reg("mod", "pskmod",    &comm::detail::pskmod_reg);
    reg("mod", "pskdemod",  &comm::detail::pskdemod_reg);
    reg("mod", "dpskmod",   &comm::detail::dpskmod_reg);
    reg("mod", "dpskdemod", &comm::detail::dpskdemod_reg);
}

} // namespace numkit
