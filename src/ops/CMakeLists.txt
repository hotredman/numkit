# ops/CMakeLists.txt
#
# numkit_ops — reusable numeric primitives shared by the VM (core) and the
# pure toolboxes (libs):
#   • element-wise op kernels (plus/minus/times/rdivide, matmul, compare)
#     with portable + Highway-SIMD backends, selected by NUMKIT_WITH_SIMD;
#   • parallel_for + thread_pool (the parallel-execution infra those kernels
#     and the toolboxes use).
#
# L0.5 layer: depends on numkit_value (Value), Highway (under SIMD) and
# Threads (under NUMKIT_WITH_THREADS) — never on core, fs, or any toolbox.

# Always-on primitives (no SIMD/threads gating).
set(NUMKIT_OPS_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/matlab_mt19937.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/rng.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/la_solve.cpp
    # Iterative solver kernels (Brent root/bracket/min, Nelder-Mead; FnHandle
    # objective) — the numerical core of fzero/fminbnd/fminsearch. Portable C++.
    ${CMAKE_CURRENT_SOURCE_DIR}/src/root_solve.cpp
    # Direct (time-domain) FIR convolution — the kernel behind signal conv()/
    # xcorr(). Portable C++; a Highway backend can replace the inner accumulate.
    ${CMAKE_CURRENT_SOURCE_DIR}/src/conv.cpp
    # Complex-input fused kernels — scalar std::complex (no SIMD), needed in
    # both the SIMD and portable builds.
    ${CMAKE_CURRENT_SOURCE_DIR}/src/fused/fused_complex.cpp
    # Shared scalar bodies for the cheap element-wise ops that carry a small-N
    # inline gate. Compiled in both builds: the portable fallbacks forward here
    # and the Highway dispatchers' small-N gate calls these (one body, no drift;
    # lambda-free TU so it auto-vectorizes) — see bugs/ops/cheap-elementwise-simd-small-n.
    ${CMAKE_CURRENT_SOURCE_DIR}/src/binary_scalar.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/fused/fused_scalar.cpp
    # Public kernel facade (numkit/ops/kernels.hpp) — stable forwarders over the
    # backend-split detail loops; the codegen lowering target. Base TU (no SIMD
    # specialisation of its own; resolves to the linked backend).
    ${CMAKE_CURRENT_SOURCE_DIR}/src/kernels.cpp)
if(NUMKIT_WITH_THREADS)
    list(APPEND NUMKIT_OPS_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/thread_pool.cpp)
endif()
if(NUMKIT_WITH_SIMD)
    list(APPEND NUMKIT_OPS_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/binary_ops_highway.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/compare_highway.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/fused/fused_affine_clamp_highway.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/fused/fused_affine_highway.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/fused/fused_axpby_highway.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/fused/fused_shift_scale_highway.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/fused/fused_abs_highway.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/fused/fused_unary_affine_highway.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/fused/fused_sq_highway.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/fused/fused_soft_threshold_highway.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/fused/fused_trans_affine_highway.cpp
        # Backend-split FFT kernel — the SIMD path splits across several TUs
        # (one per radix family) so MSVC's per-TU inliner budget isn't shared
        # across butterflies; see feedback_fft_msvc_limits memory.
        ${CMAKE_CURRENT_SOURCE_DIR}/src/fft/fft_simd.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/fft/fft_r2_simd.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/fft/fft_r4_simd.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/fft/fft_stockham_simd.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/fft/fft_r2_soa_simd.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/fft/fft_r4_soa_simd.cpp)
else()
    list(APPEND NUMKIT_OPS_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/src/binary_ops_portable.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/compare_portable.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/fused/fused_kernels_portable.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/fft/fft_portable.cpp)
endif()

add_library(numkit_ops STATIC ${NUMKIT_OPS_SOURCES})

target_include_directories(numkit_ops
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        # Resolution root for the Highway HWY_TARGET_INCLUDE strings
        # ("binary_ops_highway.cpp" / "compare_highway.cpp").
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(numkit_ops PUBLIC numkit_value)
target_compile_features(numkit_ops PUBLIC cxx_std_17)
set_target_properties(numkit_ops PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(EMSCRIPTEN)
    target_compile_options(numkit_ops PRIVATE -fexceptions)
endif()

if(NUMKIT_WITH_SIMD)
    # PRIVATE + BUILD_INTERFACE: Highway is a pure implementation detail and
    # stays out of the exported target set (mirrors the numkit aggregate).
    target_link_libraries(numkit_ops PRIVATE
        $<BUILD_INTERFACE:hwy>
        $<BUILD_INTERFACE:hwy_contrib>)
    target_compile_definitions(numkit_ops PUBLIC NUMKIT_WITH_SIMD=1)
endif()

if(NUMKIT_WITH_THREADS)
    find_package(Threads REQUIRED)
    target_link_libraries(numkit_ops PUBLIC Threads::Threads)
    target_compile_definitions(numkit_ops PUBLIC NUMKIT_WITH_THREADS=1)
endif()
