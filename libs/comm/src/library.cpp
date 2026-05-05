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

// channel/channel.cpp
void awgn_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);
void wgn_reg        (Span<const Value>, size_t, Span<Value>, CallContext &);
void bsc_reg        (Span<const Value>, size_t, Span<Value>, CallContext &);
void qfunc_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void qfuncinv_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void marcumq_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void berawgn_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void berconfint_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void noisebw_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void convertSNR_reg (Span<const Value>, size_t, Span<Value>, CallContext &);

// channel/fading.cpp
void rayleighchan_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void ricianchan_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);

// eq/pulse.cpp
void rcosdesign_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void gaussdesign_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void rectpulse_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void intdump_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);

// eq/scrambler.cpp
void scrambler_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void descrambler_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
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

    reg("rf", "awgn",        &comm::detail::awgn_reg);
    reg("rf", "wgn",         &comm::detail::wgn_reg);
    reg("rf", "bsc",         &comm::detail::bsc_reg);
    reg("perf", "qfunc",     &comm::detail::qfunc_reg);
    reg("perf", "qfuncinv",  &comm::detail::qfuncinv_reg);
    reg("perf", "marcumq",   &comm::detail::marcumq_reg);
    reg("perf", "berawgn",     &comm::detail::berawgn_reg);
    reg("perf", "berconfint",  &comm::detail::berconfint_reg);
    reg("perf", "noisebw",     &comm::detail::noisebw_reg);
    reg("perf", "convertSNR",&comm::detail::convertSNR_reg);

    reg("rf", "rayleighchan", &comm::detail::rayleighchan_reg);
    reg("rf", "ricianchan",   &comm::detail::ricianchan_reg);

    reg("eq", "rcosdesign",  &comm::detail::rcosdesign_reg);
    reg("eq", "gaussdesign", &comm::detail::gaussdesign_reg);
    reg("eq", "rectpulse",   &comm::detail::rectpulse_reg);
    reg("eq", "intdump",     &comm::detail::intdump_reg);
    reg("eq", "scrambler",   &comm::detail::scrambler_reg);
    reg("eq", "descrambler", &comm::detail::descrambler_reg);
}

} // namespace numkit
