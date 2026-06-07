set(SIGNALLIB_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/library.cpp
    # waveform_generation/
    ${CMAKE_CURRENT_SOURCE_DIR}/waveform_generation/waveform_generation.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/waveform_generation/waveform_generation_reg.cpp # register (CallContext)
    # filter_design/
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_design/filter_design.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_design/filter_design_reg.cpp # register (CallContext)
    # filter_analysis/
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_analysis/frequency_response.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_analysis/frequency_response_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_analysis/unwrap.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_analysis/unwrap_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_analysis/responses.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_analysis/responses_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_analysis/predicates.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_analysis/predicates_reg.cpp # register (CallContext)
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
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_implementation/conversions.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_implementation/conversions_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_implementation/conversions_extras.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_implementation/conversions_extras_reg.cpp # register (CallContext)
    # multirate/
    ${CMAKE_CURRENT_SOURCE_DIR}/multirate/multirate.cpp   # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/multirate/multirate_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/multirate/extras.cpp      # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/multirate/extras_reg.cpp # register (CallContext)
    # convolution/
    ${CMAKE_CURRENT_SOURCE_DIR}/convolution/convolution.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/convolution/convolution_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/convolution/extras.cpp    # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/convolution/extras_reg.cpp # register (CallContext)
    # transforms/
    ${CMAKE_CURRENT_SOURCE_DIR}/transforms/fft.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/transforms/fft_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/transforms/hilbert.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/transforms/hilbert_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/transforms/dct.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/transforms/dct_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/transforms/goertzel.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/transforms/goertzel_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/transforms/transform_helpers.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/transforms/transform_helpers_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/transforms/extras.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/transforms/extras_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/transforms/fwht.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/transforms/fwht_reg.cpp # register (CallContext)
    # smoothing/
    ${CMAKE_CURRENT_SOURCE_DIR}/smoothing/sgolay.cpp      # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/smoothing/sgolay_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/smoothing/medfilt.cpp     # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/smoothing/medfilt_reg.cpp # register (CallContext)
    # measurements/
    ${CMAKE_CURRENT_SOURCE_DIR}/measurements/findpeaks.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/measurements/findpeaks_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/measurements/dbconv.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/measurements/dbconv_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/measurements/signal_stats.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/measurements/signal_stats_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/measurements/pulse_metrics.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/measurements/pulse_metrics_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/measurements/vibration.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/measurements/vibration_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/measurements/sigroi.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/measurements/sigroi_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/measurements/sig_utils.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/measurements/sig_utils_reg.cpp # register (CallContext)
    # spectral_analysis/
    ${CMAKE_CURRENT_SOURCE_DIR}/spectral_analysis/periodogram_pwelch.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/spectral_analysis/periodogram_pwelch_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/spectral_analysis/spectral_metrics.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/spectral_analysis/spectral_metrics_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/spectral_analysis/signal_modeling.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/spectral_analysis/signal_modeling_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/spectral_analysis/ar.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/spectral_analysis/ar_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_design/analog_filters.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_design/analog_filters_reg.cpp # register (CallContext)
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_design/iir_designs.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/filter_design/iir_designs_reg.cpp # register (CallContext)
    # time_frequency/
    ${CMAKE_CURRENT_SOURCE_DIR}/time_frequency/spectrogram.cpp # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/time_frequency/spectrogram_reg.cpp # register (CallContext)
    # windows/
    ${CMAKE_CURRENT_SOURCE_DIR}/windows/windows.cpp       # compute (engine-free)
    ${CMAKE_CURRENT_SOURCE_DIR}/windows/windows_reg.cpp # register (CallContext)
)

# The backend-split FFT kernel (radix-2/4 / Stockham / SoA, portable + SIMD
# TUs) moved to ops/ (numkit_ops) — a reusable raw-buffer transform primitive.
# transforms/fft.cpp (the Value-level fft/ifft) stays here and calls it via the
# numkit::signal::detail re-export shim (backends/fft_kernels.hpp).

set(SIGNALLIB_SOURCES ${SIGNALLIB_SOURCES} PARENT_SCOPE)
