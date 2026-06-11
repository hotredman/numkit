// toolboxes/audio/src/library.cpp

#include <numkit/audio/library.hpp>

#include <numkit/core/types.hpp>

namespace numkit::audio::detail {
// scale/freq_scales.cpp
void hz2mel_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void mel2hz_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void hz2bark_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void bark2hz_reg  (Span<const Value>, size_t, Span<Value>, CallContext &);
void hz2erb_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void erb2hz_reg   (Span<const Value>, size_t, Span<Value>, CallContext &);
void phon2sone_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void sone2phon_reg(Span<const Value>, size_t, Span<Value>, CallContext &);

// spectral/shape_descriptors.cpp
void spectralCentroid_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void spectralSpread_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void spectralRolloffPoint_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void spectralDecrease_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void spectralSlope_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);
void spectralFlux_reg        (Span<const Value>, size_t, Span<Value>, CallContext &);
void spectralCrest_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);
void spectralEntropy_reg     (Span<const Value>, size_t, Span<Value>, CallContext &);
void spectralFlatness_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void spectralKurtosis_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);
void spectralSkewness_reg    (Span<const Value>, size_t, Span<Value>, CallContext &);

// spectral/melspec_delta.cpp
void melSpectrogram_reg      (Span<const Value>, size_t, Span<Value>, CallContext &);
void audioDelta_reg          (Span<const Value>, size_t, Span<Value>, CallContext &);

// spectral/cepstral.cpp
void cepstralCoefficients_reg(Span<const Value>, size_t, Span<Value>, CallContext &);
void mfcc_reg                (Span<const Value>, size_t, Span<Value>, CallContext &);
void gtcc_reg                (Span<const Value>, size_t, Span<Value>, CallContext &);

// features/pitch_harmonics.cpp
void pitch_reg               (Span<const Value>, size_t, Span<Value>, CallContext &);
void harmonicRatio_reg       (Span<const Value>, size_t, Span<Value>, CallContext &);
} // namespace numkit::audio::detail

namespace numkit {

void AudioLibrary::install(Engine &engine)
{
    auto reg = [&](const char *sub, const char *name, ExternalFunc fn) {
        engine.registerFunction(std::string("audio.") + sub, name, fn);
        engine.registerFunction("compat", name, fn);
    };

    reg("scale", "hz2mel",     &audio::detail::hz2mel_reg);
    reg("scale", "mel2hz",     &audio::detail::mel2hz_reg);
    reg("scale", "hz2bark",    &audio::detail::hz2bark_reg);
    reg("scale", "bark2hz",    &audio::detail::bark2hz_reg);
    reg("scale", "hz2erb",     &audio::detail::hz2erb_reg);
    reg("scale", "erb2hz",     &audio::detail::erb2hz_reg);
    reg("scale", "phon2sone",  &audio::detail::phon2sone_reg);
    reg("scale", "sone2phon",  &audio::detail::sone2phon_reg);

    reg("spectral", "spectralCentroid",     &audio::detail::spectralCentroid_reg);
    reg("spectral", "spectralSpread",       &audio::detail::spectralSpread_reg);
    reg("spectral", "spectralRolloffPoint", &audio::detail::spectralRolloffPoint_reg);
    reg("spectral", "spectralDecrease",     &audio::detail::spectralDecrease_reg);
    reg("spectral", "spectralSlope",        &audio::detail::spectralSlope_reg);
    reg("spectral", "spectralFlux",         &audio::detail::spectralFlux_reg);

    // Cycle I: per-frame STFT MATLAB-named camelCase variants. The
    // toolboxes/signal lowercase forms still exist as legacy single-segment
    // scalar versions (signal.spectral_analysis.spectralcrest etc.).
    reg("spectral", "spectralCrest",        &audio::detail::spectralCrest_reg);
    reg("spectral", "spectralEntropy",      &audio::detail::spectralEntropy_reg);
    reg("spectral", "spectralFlatness",     &audio::detail::spectralFlatness_reg);
    reg("spectral", "spectralKurtosis",     &audio::detail::spectralKurtosis_reg);
    reg("spectral", "spectralSkewness",     &audio::detail::spectralSkewness_reg);

    reg("spectral", "melSpectrogram",       &audio::detail::melSpectrogram_reg);
    reg("features", "audioDelta",           &audio::detail::audioDelta_reg);

    reg("features", "cepstralCoefficients", &audio::detail::cepstralCoefficients_reg);
    reg("features", "mfcc",                 &audio::detail::mfcc_reg);
    reg("features", "gtcc",                 &audio::detail::gtcc_reg);

    reg("features", "pitch",                &audio::detail::pitch_reg);
    reg("features", "harmonicRatio",        &audio::detail::harmonicRatio_reg);
}

} // namespace numkit
