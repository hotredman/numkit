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

// modulation/analog.cpp
void pmmod_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void ammod_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void fmmod_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void ssbmod_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void mskmod_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);

// modulation/generic_qam.cpp
void genqammod_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void genqamdemod_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);

// modulation/apsk.cpp
void apskmod_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);
void apskdemod_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);

// modulation/mil188.cpp
void mil188qammod_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void mil188qamdemod_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);

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

// eq/errors.cpp
void biterr_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);
void symerr_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);

// eq/compand.cpp
void compand_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);

// source/random_source.cpp
void randsrc_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void randerr_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);

// source/huffman.cpp
void huffmandict_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void huffmanenco_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void huffmandeco_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);

// source/dpcm.cpp
void dpcmenco_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void dpcmdeco_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);

// source/quantiz.cpp
void quantiz_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);

// source/lloyds.cpp
void lloyds_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);

// source/dpcmopt.cpp
void dpcmopt_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);

// source/arithcoding.cpp
void arithenco_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void arithdeco_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);

// source/base_conversions.cpp
void bit2int_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void int2bit_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void bi2de_reg        (Span<const Value>, size_t, Span<Value>, CallContext &);
void de2bi_reg        (Span<const Value>, size_t, Span<Value>, CallContext &);
void vec2mat_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);

// coding/convcoding.cpp
void poly2trellis_reg (Span<const Value>, size_t, Span<Value>, CallContext &);
void convenc_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void vitdec_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);
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

    reg("mod", "pmmod",     &comm::detail::pmmod_reg);
    reg("mod", "ammod",     &comm::detail::ammod_reg);
    reg("mod", "fmmod",     &comm::detail::fmmod_reg);
    reg("mod", "ssbmod",    &comm::detail::ssbmod_reg);
    reg("mod", "mskmod",    &comm::detail::mskmod_reg);

    reg("mod", "genqammod",   &comm::detail::genqammod_reg);
    reg("mod", "genqamdemod", &comm::detail::genqamdemod_reg);
    reg("mod", "apskmod",     &comm::detail::apskmod_reg);
    reg("mod", "apskdemod",   &comm::detail::apskdemod_reg);
    reg("mod", "mil188qammod",   &comm::detail::mil188qammod_reg);
    reg("mod", "mil188qamdemod", &comm::detail::mil188qamdemod_reg);

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
    reg("eq", "biterr",      &comm::detail::biterr_reg);
    reg("eq", "symerr",      &comm::detail::symerr_reg);
    reg("eq", "compand",     &comm::detail::compand_reg);

    reg("rf", "randsrc",     &comm::detail::randsrc_reg);
    reg("rf", "randerr",     &comm::detail::randerr_reg);

    reg("eq", "huffmandict", &comm::detail::huffmandict_reg);
    reg("eq", "huffmanenco", &comm::detail::huffmanenco_reg);
    reg("eq", "huffmandeco", &comm::detail::huffmandeco_reg);

    reg("eq", "dpcmenco",    &comm::detail::dpcmenco_reg);
    reg("eq", "dpcmdeco",    &comm::detail::dpcmdeco_reg);
    reg("eq", "quantiz",     &comm::detail::quantiz_reg);
    reg("eq", "lloyds",      &comm::detail::lloyds_reg);
    reg("eq", "dpcmopt",     &comm::detail::dpcmopt_reg);

    reg("eq", "arithenco",   &comm::detail::arithenco_reg);
    reg("eq", "arithdeco",   &comm::detail::arithdeco_reg);

    // base conversions (signal-style utilities)
    reg("rf", "bit2int",     &comm::detail::bit2int_reg);
    reg("rf", "int2bit",     &comm::detail::int2bit_reg);
    reg("rf", "bi2de",       &comm::detail::bi2de_reg);
    reg("rf", "de2bi",       &comm::detail::de2bi_reg);
    reg("rf", "vec2mat",     &comm::detail::vec2mat_reg);

    // ── Error Correction Codes: convolutional coding ──
    reg("coding", "poly2trellis", &comm::detail::poly2trellis_reg);
    reg("coding", "convenc",      &comm::detail::convenc_reg);
    reg("coding", "vitdec",       &comm::detail::vitdec_reg);
}

} // namespace numkit
