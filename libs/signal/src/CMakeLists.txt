set(SIGNALLIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/library.cpp
    # waveform_generation/
    ${CMAKE_CURRENT_SOURCE_DIR}/waveform_generation/waveform_generation.cpp
    # filter_design/
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_design/filter_design.cpp
    # filter_analysis/
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_analysis/frequency_response.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_analysis/unwrap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_analysis/responses.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_analysis/predicates.cpp
    # digital_filtering/
    ${CMAKE_CURRENT_SOURCE_DIR}/digital_filtering/buffer.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/digital_filtering/buffer_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/digital_filtering/filter.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/digital_filtering/filter_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/digital_filtering/poly_utils.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/digital_filtering/poly_utils_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/digital_filtering/quantizers.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/digital_filtering/quantizers_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/digital_filtering/shiftdata.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/digital_filtering/shiftdata_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/digital_filtering/sosfilt.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/digital_filtering/sosfilt_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/digital_filtering/spec_driven.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/digital_filtering/spec_driven_reg.cpp # register (CallContext)
    # filter_implementation/
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_implementation/conversions.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_implementation/conversions_extras.cpp
    # multirate/
    ${CMAKE_CURRENT_SOURCE_DIR}/multirate/multirate.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/multirate/extras.cpp
    # convolution/
    ${CMAKE_CURRENT_SOURCE_DIR}/convolution/convolution.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/convolution/convolution_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/convolution/extras.cpp    # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/convolution/extras_reg.cpp # register (CallContext)
    # transforms/
    ${CMAKE_CURRENT_SOURCE_DIR}/transforms/fft.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/transforms/hilbert.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/transforms/dct.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/transforms/goertzel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/transforms/transform_helpers.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/transforms/extras.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/transforms/fwht.cpp
    # smoothing/
    ${CMAKE_CURRENT_SOURCE_DIR}/smoothing/sgolay.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/smoothing/medfilt.cpp
    # measurements/
    ${CMAKE_CURRENT_SOURCE_DIR}/measurements/findpeaks.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/measurements/dbconv.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/measurements/signal_stats.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/measurements/pulse_metrics.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/measurements/vibration.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/measurements/sigroi.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/measurements/sig_utils.cpp
    # spectral_analysis/
    ${CMAKE_CURRENT_SOURCE_DIR}/spectral_analysis/periodogram_pwelch.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/spectral_analysis/spectral_metrics.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/spectral_analysis/signal_modeling.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/spectral_analysis/ar.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_design/analog_filters.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_design/iir_designs.cpp
    # time_frequency/
    ${CMAKE_CURRENT_SOURCE_DIR}/time_frequency/spectrogram.cpp
    # windows/
    ${CMAKE_CURRENT_SOURCE_DIR}/windows/windows.cpp
)

# The backend-split FFT kernel (radix-2/4 / Stockham / SoA, portable + SIMD
# TUs) moved to ops/ (numkit_ops) — a reusable raw-buffer transform primitive.
# transforms/fft.cpp (the Value-level fft/ifft) stays here and calls it via the
# numkit::signal::detail re-export shim (backends/fft_kernels.hpp).

set(SIGNALLIB_SOURCES ${SIGNALLIB_SOURCES} PARENT_SCOPE)
