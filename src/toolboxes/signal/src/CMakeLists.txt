set(SIGNALLIB_SOURCES
    # waveform_generation/
    ${CMAKE_CURRENT_SOURCE_DIR}/waveform_generation/waveform_generation.cpp # compute (engine-free)
    # filter_design/
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_design/filter_design.cpp # compute (engine-free)
    # filter_analysis/
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_analysis/frequency_response.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_analysis/unwrap.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_analysis/responses.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_analysis/predicates.cpp # compute (engine-free)
    # digital_filtering/
    ${CMAKE_CURRENT_SOURCE_DIR}/digital_filtering/buffer.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/digital_filtering/filter.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/digital_filtering/poly_utils.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/digital_filtering/quantizers.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/digital_filtering/shiftdata.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/digital_filtering/sosfilt.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/digital_filtering/spec_driven.cpp # compute (engine-free)
    # filter_implementation/
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_implementation/conversions.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_implementation/conversions_extras.cpp # compute (engine-free)
    # multirate/
    ${CMAKE_CURRENT_SOURCE_DIR}/multirate/multirate.cpp   # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/multirate/extras.cpp      # compute (engine-free)
    # convolution/
    ${CMAKE_CURRENT_SOURCE_DIR}/convolution/convolution.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/convolution/extras.cpp    # compute (engine-free)
    # transforms/
    ${CMAKE_CURRENT_SOURCE_DIR}/transforms/fft.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/transforms/hilbert.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/transforms/dct.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/transforms/goertzel.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/transforms/transform_helpers.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/transforms/extras.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/transforms/fwht.cpp # compute (engine-free)
    # smoothing/
    ${CMAKE_CURRENT_SOURCE_DIR}/smoothing/sgolay.cpp      # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/smoothing/medfilt.cpp     # compute (engine-free)
    # measurements/
    ${CMAKE_CURRENT_SOURCE_DIR}/measurements/findpeaks.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/measurements/dbconv.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/measurements/signal_stats.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/measurements/pulse_metrics.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/measurements/vibration.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/measurements/sigroi.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/measurements/sig_utils.cpp # compute (engine-free)
    # spectral_analysis/
    ${CMAKE_CURRENT_SOURCE_DIR}/spectral_analysis/periodogram_pwelch.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/spectral_analysis/spectral_metrics.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/spectral_analysis/signal_modeling.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/spectral_analysis/pseudospectrum.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/spectral_analysis/ar.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_design/analog_filters.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_design/iir_designs.cpp # compute (engine-free)
    # time_frequency/
    ${CMAKE_CURRENT_SOURCE_DIR}/time_frequency/spectrogram.cpp # compute (engine-free)
    # windows/
    ${CMAKE_CURRENT_SOURCE_DIR}/windows/windows.cpp       # compute (engine-free)
)

# The backend-split FFT kernel (radix-2/4 / Stockham / SoA, portable + SIMD
# TUs) moved to ops/ (numkit_ops) — a reusable raw-buffer transform primitive.
# transforms/fft.cpp (the Value-level fft/ifft) stays here and calls it via the
# numkit::signal::detail re-export shim (backends/fft_kernels.hpp).

set(SIGNALLIB_SOURCES ${SIGNALLIB_SOURCES} PARENT_SCOPE)
