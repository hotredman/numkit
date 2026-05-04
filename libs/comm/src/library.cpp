// libs/comm/src/library.cpp

#include <numkit/comm/library.hpp>

#include <numkit/core/types.hpp>

namespace numkit::comm::detail {
// modulation/psk.cpp
void pskmod_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void pskdemod_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void dpskmod_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void dpskdemod_reg (Span<const Value>, size_t, Span<Value>, CallContext &);

// modulation/qam.cpp
void pammod_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void pamdemod_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void qammod_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void qamdemod_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void modnorm_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);

// modulation/fsk_ofdm.cpp
void fskmod_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void fskdemod_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void ofdmmod_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void ofdmdemod_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
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

    reg("mod", "pammod",    &comm::detail::pammod_reg);
    reg("mod", "pamdemod",  &comm::detail::pamdemod_reg);
    reg("mod", "qammod",    &comm::detail::qammod_reg);
    reg("mod", "qamdemod",  &comm::detail::qamdemod_reg);
    reg("mod", "modnorm",   &comm::detail::modnorm_reg);

    reg("mod", "fskmod",    &comm::detail::fskmod_reg);
    reg("mod", "fskdemod",  &comm::detail::fskdemod_reg);
    reg("mod", "ofdmmod",   &comm::detail::ofdmmod_reg);
    reg("mod", "ofdmdemod", &comm::detail::ofdmdemod_reg);
}

} // namespace numkit
