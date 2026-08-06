#include <numkit/ops/blas.hpp>
#include <numkit/ops/parallel_for.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>
#include <atomic>

#undef HWY_TARGET_INCLUDE
#define HWY_TARGET_INCLUDE "blas/gemm_highway.cpp"
#include <hwy/foreach_target.h>
#include <hwy/highway.h>

HWY_BEFORE_NAMESPACE();
namespace numkit::ops {
namespace HWY_NAMESPACE {

namespace hn = hwy::HWY_NAMESPACE;

void GemmDoubleKernel(std::size_t m, std::size_t n, std::size_t k,
                      double alpha, const double *A, std::size_t lda,
                      const double *B, std::size_t ldb,
                      double beta, double *C, std::size_t ldc)
{
    if (m == 0 || n == 0) return;

    // Handle beta scaling when k == 0 or alpha == 0
    if (k == 0 || alpha == 0.0) {
        if (beta == 0.0) {
            for (std::size_t j = 0; j < n; ++j) {
                for (std::size_t i = 0; i < m; ++i) {
                    C[i + j * ldc] = 0.0;
                }
            }
        } else if (beta != 1.0) {
            for (std::size_t j = 0; j < n; ++j) {
                for (std::size_t i = 0; i < m; ++i) {
                    C[i + j * ldc] *= beta;
                }
            }
        }
        ::numkit::ops::g_last_gemm_threads_used.store(1);
        return;
    }

    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);

    // BLIS blocking parameters
    constexpr std::size_t mr_vec = 2;              // 2 vector rows (2 * N)
    const std::size_t mr = mr_vec * N;              // e.g. 8 doubles for AVX2, 16 for AVX-512
    constexpr std::size_t nr = 6;                   // 6 columns
    constexpr std::size_t kc_block = 256;           // L2-resident A-panel width
    constexpr std::size_t mc_block = 256;           // L2-resident A-panel height
    constexpr std::size_t nc_block = 2048;          // L3-resident B-panel width

    // Zero-allocation stack-based fast path for small matrices (m, n, k <= 128)
    if (m <= 128 && n <= 128 && k <= 128) {
        alignas(64) double A_pack[144 * 128 + 64];
        alignas(64) double B_pack[144 * 128 + 64];

        const std::size_t mc = m;
        const std::size_t nc = n;
        const std::size_t kc = k;

        // Pack B (kc x nc) into row-panels of width nr
        for (std::size_t jr = 0; jr < nc; jr += nr) {
            std::size_t cur_nr = std::min(nr, nc - jr);
            double *b_p = B_pack + jr * kc;
            for (std::size_t kk = 0; kk < kc; ++kk) {
                for (std::size_t c = 0; c < cur_nr; ++c) {
                    b_p[kk * nr + c] = B[kk + (jr + c) * ldb];
                }
                for (std::size_t c = cur_nr; c < nr; ++c) {
                    b_p[kk * nr + c] = 0.0;
                }
            }
        }

        // Pack A (mc x kc) into column-panels of height mr
        for (std::size_t ii = 0; ii < mc; ii += mr) {
            std::size_t cur_mr = std::min(mr, mc - ii);
            double *a_p = A_pack + ii * kc;
            for (std::size_t kk = 0; kk < kc; ++kk) {
                const double *a_col = A + ii + kk * lda;
                for (std::size_t r = 0; r < cur_mr; ++r) {
                    a_p[kk * mr + r] = a_col[r];
                }
                for (std::size_t r = cur_mr; r < mr; ++r) {
                    a_p[kk * mr + r] = 0.0;
                }
            }
        }

        // SIMD Highway Kernel over C
        for (std::size_t jr = 0; jr < nc; jr += nr) {
            std::size_t cur_nr = std::min(nr, nc - jr);
            const double *b_p_tile = B_pack + jr * kc;

            for (std::size_t ir = 0; ir < mc; ir += mr) {
                std::size_t cur_mr = std::min(mr, mc - ir);
                const double *a_p = A_pack + ir * kc;
                double *c_ptr = C + ir + jr * ldc;

                if (cur_mr == mr && cur_nr == nr) {
                    auto c00 = hn::Zero(d), c01 = hn::Zero(d), c02 = hn::Zero(d), c03 = hn::Zero(d), c04 = hn::Zero(d), c05 = hn::Zero(d);
                    auto c10 = hn::Zero(d), c11 = hn::Zero(d), c12 = hn::Zero(d), c13 = hn::Zero(d), c14 = hn::Zero(d), c15 = hn::Zero(d);

                    for (std::size_t kk = 0; kk < kc; ++kk) {
                        auto a0 = hn::LoadU(d, a_p + kk * mr + 0 * N);
                        auto a1 = hn::LoadU(d, a_p + kk * mr + 1 * N);

                        const double *b_col = b_p_tile + kk * nr;
                        auto b0 = hn::Set(d, b_col[0]);
                        auto b1 = hn::Set(d, b_col[1]);
                        auto b2 = hn::Set(d, b_col[2]);
                        auto b3 = hn::Set(d, b_col[3]);
                        auto b4 = hn::Set(d, b_col[4]);
                        auto b5 = hn::Set(d, b_col[5]);

                        c00 = hn::MulAdd(a0, b0, c00); c10 = hn::MulAdd(a1, b0, c10);
                        c01 = hn::MulAdd(a0, b1, c01); c11 = hn::MulAdd(a1, b1, c11);
                        c02 = hn::MulAdd(a0, b2, c02); c12 = hn::MulAdd(a1, b2, c12);
                        c03 = hn::MulAdd(a0, b3, c03); c13 = hn::MulAdd(a1, b3, c13);
                        c04 = hn::MulAdd(a0, b4, c04); c14 = hn::MulAdd(a1, b4, c14);
                        c05 = hn::MulAdd(a0, b5, c05); c15 = hn::MulAdd(a1, b5, c15);
                    }

                    auto v_alpha = hn::Set(d, alpha);
                    auto v_beta = hn::Set(d, beta);

                    #define STORE_COL_FAST(col_idx, acc0, acc1) \
                    { \
                        double *col_c = c_ptr + col_idx * ldc; \
                        auto r0 = hn::Mul(acc0, v_alpha); \
                        auto r1 = hn::Mul(acc1, v_alpha); \
                        if (beta != 0.0) { \
                            r0 = hn::MulAdd(hn::LoadU(d, col_c + 0 * N), v_beta, r0); \
                            r1 = hn::MulAdd(hn::LoadU(d, col_c + 1 * N), v_beta, r1); \
                        } \
                        hn::StoreU(r0, d, col_c + 0 * N); \
                        hn::StoreU(r1, d, col_c + 1 * N); \
                    }

                    STORE_COL_FAST(0, c00, c10);
                    STORE_COL_FAST(1, c01, c11);
                    STORE_COL_FAST(2, c02, c12);
                    STORE_COL_FAST(3, c03, c13);
                    STORE_COL_FAST(4, c04, c14);
                    STORE_COL_FAST(5, c05, c15);
                    #undef STORE_COL_FAST
                } else {
                    for (std::size_t c = 0; c < cur_nr; ++c) {
                        double *col_c = c_ptr + c * ldc;
                        const double *b_col = b_p_tile + c;
                        for (std::size_t r = 0; r < cur_mr; ++r) {
                            double acc = 0.0;
                            for (std::size_t kk = 0; kk < kc; ++kk) {
                                acc += a_p[kk * mr + r] * b_col[kk * nr];
                            }
                            if (beta == 0.0) {
                                col_c[r] = alpha * acc;
                            } else {
                                col_c[r] = beta * col_c[r] + alpha * acc;
                            }
                        }
                    }
                }
            }
        }

        ::numkit::ops::g_last_gemm_threads_used.store(1);
        return;
    }

    // Threshold for FLOPs: 2 * m * n * k >= 8,000,000 (n >= 160 for square GEMM)
    const std::size_t total_flops = 2 * m * n * k;
    constexpr std::size_t kGemmParallelFlopThreshold = 8'000'000;

    // Determinism note: FP summation order within a C tile is fixed by the
    // microkernel; parallelism across independent jc column blocks of C does
    // not change per-element floating point summation order or results.
    const std::size_t p_thresh = (total_flops >= kGemmParallelFlopThreshold) ? std::size_t{1} : n + 1;

    std::atomic<std::size_t> active_threads{0};

    numkit::detail::parallel_for(n, p_thresh, [&](std::size_t jc_start, std::size_t jc_end) {
        active_threads.fetch_add(1, std::memory_order_relaxed);
        // Each thread gets its own thread-local packing buffers
        std::vector<double> A_pack(mc_block * kc_block + 64, 0.0);
        std::vector<double> B_pack(kc_block * nc_block + 64, 0.0);

        for (std::size_t jc = jc_start; jc < jc_end; jc += nc_block) {
            std::size_t nc = std::min(nc_block, jc_end - jc);

            for (std::size_t pc = 0; pc < k; pc += kc_block) {
                std::size_t kc = std::min(kc_block, k - pc);
                double current_beta = (pc == 0) ? beta : 1.0;

                // Pack B panel ONCE per (jc, pc) tile
                for (std::size_t jr = 0; jr < nc; jr += nr) {
                    std::size_t cur_nr = std::min(nr, nc - jr);
                    double *b_p = B_pack.data() + jr * kc;
                    for (std::size_t kk = 0; kk < kc; ++kk) {
                        for (std::size_t c = 0; c < cur_nr; ++c) {
                            b_p[kk * nr + c] = B[(pc + kk) + (jc + jr + c) * ldb];
                        }
                        for (std::size_t c = cur_nr; c < nr; ++c) {
                            b_p[kk * nr + c] = 0.0;
                        }
                    }
                }

                for (std::size_t ic = 0; ic < m; ic += mc_block) {
                    std::size_t mc = std::min(mc_block, m - ic);

                    // Pack A block (mc x kc) into column-panels of height mr
                    for (std::size_t ii = 0; ii < mc; ii += mr) {
                        std::size_t cur_mr = std::min(mr, mc - ii);
                        double *a_p = A_pack.data() + ii * kc;
                        for (std::size_t kk = 0; kk < kc; ++kk) {
                            const double *a_col = A + (ic + ii) + (pc + kk) * lda;
                            for (std::size_t r = 0; r < cur_mr; ++r) {
                                a_p[kk * mr + r] = a_col[r];
                            }
                            for (std::size_t r = cur_mr; r < mr; ++r) {
                                a_p[kk * mr + r] = 0.0;
                            }
                        }
                    }

                    // Microkernel loop over jr (width nr) and ir (height mr)
                    for (std::size_t jr = 0; jr < nc; jr += nr) {
                        std::size_t cur_nr = std::min(nr, nc - jr);
                        const double *b_p_tile = B_pack.data() + jr * kc;

                        for (std::size_t ir = 0; ir < mc; ir += mr) {
                            std::size_t cur_mr = std::min(mr, mc - ir);
                            const double *a_p = A_pack.data() + ir * kc;
                            double *c_ptr = C + (ic + ir) + (jc + jr) * ldc;

                            if (cur_mr == mr && cur_nr == nr) {
                                // Full mr x nr SIMD microkernel (12 accumulator registers)
                                auto c00 = hn::Zero(d), c01 = hn::Zero(d), c02 = hn::Zero(d), c03 = hn::Zero(d), c04 = hn::Zero(d), c05 = hn::Zero(d);
                                auto c10 = hn::Zero(d), c11 = hn::Zero(d), c12 = hn::Zero(d), c13 = hn::Zero(d), c14 = hn::Zero(d), c15 = hn::Zero(d);

                                for (std::size_t kk = 0; kk < kc; ++kk) {
                                    auto a0 = hn::LoadU(d, a_p + kk * mr + 0 * N);
                                    auto a1 = hn::LoadU(d, a_p + kk * mr + 1 * N);

                                    const double *b_col = b_p_tile + kk * nr;
                                    auto b0 = hn::Set(d, b_col[0]);
                                    auto b1 = hn::Set(d, b_col[1]);
                                    auto b2 = hn::Set(d, b_col[2]);
                                    auto b3 = hn::Set(d, b_col[3]);
                                    auto b4 = hn::Set(d, b_col[4]);
                                    auto b5 = hn::Set(d, b_col[5]);

                                    c00 = hn::MulAdd(a0, b0, c00); c10 = hn::MulAdd(a1, b0, c10);
                                    c01 = hn::MulAdd(a0, b1, c01); c11 = hn::MulAdd(a1, b1, c11);
                                    c02 = hn::MulAdd(a0, b2, c02); c12 = hn::MulAdd(a1, b2, c12);
                                    c03 = hn::MulAdd(a0, b3, c03); c13 = hn::MulAdd(a1, b3, c13);
                                    c04 = hn::MulAdd(a0, b4, c04); c14 = hn::MulAdd(a1, b4, c14);
                                    c05 = hn::MulAdd(a0, b5, c05); c15 = hn::MulAdd(a1, b5, c15);
                                }

                                auto v_alpha = hn::Set(d, alpha);
                                auto v_beta = hn::Set(d, current_beta);

                                #define STORE_COL(col_idx, acc0, acc1) \
                                { \
                                    double *col_c = c_ptr + col_idx * ldc; \
                                    auto r0 = hn::Mul(acc0, v_alpha); \
                                    auto r1 = hn::Mul(acc1, v_alpha); \
                                    if (current_beta != 0.0) { \
                                        r0 = hn::MulAdd(hn::LoadU(d, col_c + 0 * N), v_beta, r0); \
                                        r1 = hn::MulAdd(hn::LoadU(d, col_c + 1 * N), v_beta, r1); \
                                    } \
                                    hn::StoreU(r0, d, col_c + 0 * N); \
                                    hn::StoreU(r1, d, col_c + 1 * N); \
                                }

                                STORE_COL(0, c00, c10);
                                STORE_COL(1, c01, c11);
                                STORE_COL(2, c02, c12);
                                STORE_COL(3, c03, c13);
                                STORE_COL(4, c04, c14);
                                STORE_COL(5, c05, c15);
                                #undef STORE_COL
                            } else {
                                // Scalar edge kernel for tail tiles
                                for (std::size_t c = 0; c < cur_nr; ++c) {
                                    double *col_c = c_ptr + c * ldc;
                                    const double *b_col = b_p_tile + c;
                                    for (std::size_t r = 0; r < cur_mr; ++r) {
                                        double acc = 0.0;
                                        for (std::size_t kk = 0; kk < kc; ++kk) {
                                            acc += a_p[kk * mr + r] * b_col[kk * nr];
                                        }
                                        if (current_beta == 0.0) {
                                            col_c[r] = alpha * acc;
                                        } else {
                                            col_c[r] = current_beta * col_c[r] + alpha * acc;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    });

    ::numkit::ops::g_last_gemm_threads_used.store(active_threads.load());
}



void GemmComplexKernel(std::size_t m, std::size_t n, std::size_t k,
                       std::complex<double> alpha, const std::complex<double> *A, std::size_t lda,
                       const std::complex<double> *B, std::size_t ldb,
                       std::complex<double> beta, std::complex<double> *C, std::size_t ldc)
{
    using Complex = std::complex<double>;

    if (m == 0 || n == 0) return;

    if (k == 0 || alpha == Complex(0.0, 0.0)) {
        if (beta == Complex(0.0, 0.0)) {
            for (std::size_t j = 0; j < n; ++j) {
                for (std::size_t i = 0; i < m; ++i) C[i + j * ldc] = Complex(0.0, 0.0);
            }
        } else if (beta != Complex(1.0, 0.0)) {
            for (std::size_t j = 0; j < n; ++j) {
                for (std::size_t i = 0; i < m; ++i) C[i + j * ldc] *= beta;
            }
        }
        return;
    }

    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);

    constexpr std::size_t mr_vec = 2;
    const std::size_t mr = mr_vec * N;
    constexpr std::size_t nr = 6;
    constexpr std::size_t kc_block = 256;
    constexpr std::size_t mc_block = 256;
    constexpr std::size_t nc_block = 2048;

    // Zero-allocation stack-based fast path for small complex matrices (m, n, k <= 128)
    if (m <= 128 && n <= 128 && k <= 128) {
        alignas(64) double Ar_pack[144 * 128 + 64];
        alignas(64) double Ai_pack[144 * 128 + 64];
        alignas(64) double Br_pack[144 * 128 + 64];
        alignas(64) double Bi_pack[144 * 128 + 64];

        const std::size_t mc = m;
        const std::size_t nc = n;
        const std::size_t kc = k;

        // Pack B real/imag
        for (std::size_t jr = 0; jr < nc; jr += nr) {
            std::size_t cur_nr = std::min(nr, nc - jr);
            double *br_p = Br_pack + jr * kc;
            double *bi_p = Bi_pack + jr * kc;
            for (std::size_t kk = 0; kk < kc; ++kk) {
                for (std::size_t c = 0; c < cur_nr; ++c) {
                    Complex b_val = B[kk + (jr + c) * ldb];
                    br_p[kk * nr + c] = b_val.real();
                    bi_p[kk * nr + c] = b_val.imag();
                }
                for (std::size_t c = cur_nr; c < nr; ++c) {
                    br_p[kk * nr + c] = 0.0;
                    bi_p[kk * nr + c] = 0.0;
                }
            }
        }

        // Pack A real/imag
        for (std::size_t ii = 0; ii < mc; ii += mr) {
            std::size_t cur_mr = std::min(mr, mc - ii);
            double *ar_p = Ar_pack + ii * kc;
            double *ai_p = Ai_pack + ii * kc;
            for (std::size_t kk = 0; kk < kc; ++kk) {
                const Complex *a_col = A + ii + kk * lda;
                for (std::size_t r = 0; r < cur_mr; ++r) {
                    ar_p[kk * mr + r] = a_col[r].real();
                    ai_p[kk * mr + r] = a_col[r].imag();
                }
                for (std::size_t r = cur_mr; r < mr; ++r) {
                    ar_p[kk * mr + r] = 0.0;
                    ai_p[kk * mr + r] = 0.0;
                }
            }
        }

        // SIMD 4M Kernel
        for (std::size_t jr = 0; jr < nc; jr += nr) {
            std::size_t cur_nr = std::min(nr, nc - jr);
            const double *br_p_tile = Br_pack + jr * kc;
            const double *bi_p_tile = Bi_pack + jr * kc;

            for (std::size_t ir = 0; ir < mc; ir += mr) {
                std::size_t cur_mr = std::min(mr, mc - ir);
                const double *ar_p = Ar_pack + ir * kc;
                const double *ai_p = Ai_pack + ir * kc;
                Complex *c_ptr = C + ir + jr * ldc;

                if (cur_mr == mr && cur_nr == nr) {
                    auto cr00 = hn::Zero(d), cr01 = hn::Zero(d), cr02 = hn::Zero(d), cr03 = hn::Zero(d), cr04 = hn::Zero(d), cr05 = hn::Zero(d);
                    auto cr10 = hn::Zero(d), cr11 = hn::Zero(d), cr12 = hn::Zero(d), cr13 = hn::Zero(d), cr14 = hn::Zero(d), cr15 = hn::Zero(d);
                    auto ci00 = hn::Zero(d), ci01 = hn::Zero(d), ci02 = hn::Zero(d), ci03 = hn::Zero(d), ci04 = hn::Zero(d), ci05 = hn::Zero(d);
                    auto ci10 = hn::Zero(d), ci11 = hn::Zero(d), ci12 = hn::Zero(d), ci13 = hn::Zero(d), ci14 = hn::Zero(d), ci15 = hn::Zero(d);

                    for (std::size_t kk = 0; kk < kc; ++kk) {
                        auto ar0 = hn::LoadU(d, ar_p + kk * mr + 0 * N);
                        auto ar1 = hn::LoadU(d, ar_p + kk * mr + 1 * N);
                        auto ai0 = hn::LoadU(d, ai_p + kk * mr + 0 * N);
                        auto ai1 = hn::LoadU(d, ai_p + kk * mr + 1 * N);

                        const double *br_k = br_p_tile + kk * nr;
                        const double *bi_k = bi_p_tile + kk * nr;

                        #define KERNEL_COL_FAST(col_idx, cr_acc0, cr_acc1, ci_acc0, ci_acc1) \
                        { \
                            auto br = hn::Set(d, br_k[col_idx]); \
                            auto bi = hn::Set(d, bi_k[col_idx]); \
                            cr_acc0 = hn::MulAdd(ar0, br, cr_acc0); \
                            cr_acc0 = hn::NegMulAdd(ai0, bi, cr_acc0); \
                            cr_acc1 = hn::MulAdd(ar1, br, cr_acc1); \
                            cr_acc1 = hn::NegMulAdd(ai1, bi, cr_acc1); \
                            ci_acc0 = hn::MulAdd(ar0, bi, ci_acc0); \
                            ci_acc0 = hn::MulAdd(ai0, br, ci_acc0); \
                            ci_acc1 = hn::MulAdd(ar1, bi, ci_acc1); \
                            ci_acc1 = hn::MulAdd(ai1, br, ci_acc1); \
                        }

                        KERNEL_COL_FAST(0, cr00, cr10, ci00, ci10);
                        KERNEL_COL_FAST(1, cr01, cr11, ci01, ci11);
                        KERNEL_COL_FAST(2, cr02, cr12, ci02, ci12);
                        KERNEL_COL_FAST(3, cr03, cr13, ci03, ci13);
                        KERNEL_COL_FAST(4, cr04, cr14, ci04, ci14);
                        KERNEL_COL_FAST(5, cr05, cr15, ci05, ci15);
                        #undef KERNEL_COL_FAST
                    }

                    auto v_alpha_r = hn::Set(d, alpha.real());
                    auto v_alpha_i = hn::Set(d, alpha.imag());

                    #define STORE_COMPLEX_COL_FAST(col_idx, cr0, cr1, ci0, ci1) \
                    { \
                        Complex *col_c = c_ptr + col_idx * ldc; \
                        auto yr0 = hn::Mul(cr0, v_alpha_r); \
                        yr0 = hn::NegMulAdd(ci0, v_alpha_i, yr0); \
                        auto yr1 = hn::Mul(cr1, v_alpha_r); \
                        yr1 = hn::NegMulAdd(ci1, v_alpha_i, yr1); \
                        auto yi0 = hn::Mul(cr0, v_alpha_i); \
                        yi0 = hn::MulAdd(ci0, v_alpha_r, yi0); \
                        auto yi1 = hn::Mul(cr1, v_alpha_i); \
                        yi1 = hn::MulAdd(ci1, v_alpha_r, yi1); \
                        alignas(64) double buf_r0[16], buf_r1[16], buf_i0[16], buf_i1[16]; \
                        hn::StoreU(yr0, d, buf_r0); \
                        hn::StoreU(yr1, d, buf_r1); \
                        hn::StoreU(yi0, d, buf_i0); \
                        hn::StoreU(yi1, d, buf_i1); \
                        for (std::size_t r = 0; r < N; ++r) { \
                            Complex c_val = Complex(buf_r0[r], buf_i0[r]); \
                            if (beta != Complex(0.0, 0.0)) { \
                                c_val += col_c[r + 0 * N] * beta; \
                            } \
                            col_c[r + 0 * N] = c_val; \
                        } \
                        for (std::size_t r = 0; r < N; ++r) { \
                            Complex c_val = Complex(buf_r1[r], buf_i1[r]); \
                            if (beta != Complex(0.0, 0.0)) { \
                                c_val += col_c[r + 1 * N] * beta; \
                            } \
                            col_c[r + 1 * N] = c_val; \
                        } \
                    }

                    STORE_COMPLEX_COL_FAST(0, cr00, cr10, ci00, ci10);
                    STORE_COMPLEX_COL_FAST(1, cr01, cr11, ci01, ci11);
                    STORE_COMPLEX_COL_FAST(2, cr02, cr12, ci02, ci12);
                    STORE_COMPLEX_COL_FAST(3, cr03, cr13, ci03, ci13);
                    STORE_COMPLEX_COL_FAST(4, cr04, cr14, ci04, ci14);
                    STORE_COMPLEX_COL_FAST(5, cr05, cr15, ci05, ci15);
                    #undef STORE_COMPLEX_COL_FAST
                } else {
                    for (std::size_t c = 0; c < cur_nr; ++c) {
                        Complex *col_c = c_ptr + c * ldc;
                        const double *br_col = br_p_tile + c;
                        const double *bi_col = bi_p_tile + c;
                        for (std::size_t r = 0; r < cur_mr; ++r) {
                            Complex acc(0.0, 0.0);
                            for (std::size_t kk = 0; kk < kc; ++kk) {
                                Complex a_val(ar_p[kk * mr + r], ai_p[kk * mr + r]);
                                Complex b_val(br_col[kk * nr], bi_col[kk * nr]);
                                acc += a_val * b_val;
                            }
                            if (beta == Complex(0.0, 0.0)) {
                                col_c[r] = alpha * acc;
                            } else {
                                col_c[r] = beta * col_c[r] + alpha * acc;
                            }
                        }
                    }
                }
            }
        }
        return;
    }

    const std::size_t total_flops = 8 * m * n * k;
    constexpr std::size_t kGemmParallelFlopThreshold = 8'000'000;
    const std::size_t p_thresh = (total_flops >= kGemmParallelFlopThreshold) ? std::size_t{1} : n + 1;

    numkit::detail::parallel_for(n, p_thresh, [=](std::size_t jc_start, std::size_t jc_end) {
        std::vector<double> Ar_pack(mc_block * kc_block + 64, 0.0);
        std::vector<double> Ai_pack(mc_block * kc_block + 64, 0.0);
        std::vector<double> Br_pack(kc_block * nc_block + 64, 0.0);
        std::vector<double> Bi_pack(kc_block * nc_block + 64, 0.0);

        for (std::size_t jc = jc_start; jc < jc_end; jc += nc_block) {
            std::size_t nc = std::min(nc_block, jc_end - jc);

            for (std::size_t pc = 0; pc < k; pc += kc_block) {
                std::size_t kc = std::min(kc_block, k - pc);
                Complex current_beta = (pc == 0) ? beta : Complex(1.0, 0.0);

                // Pack B real and imaginary blocks ONCE per (jc, pc) tile
                for (std::size_t jr = 0; jr < nc; jr += nr) {
                    std::size_t cur_nr = std::min(nr, nc - jr);
                    double *br_p = Br_pack.data() + jr * kc;
                    double *bi_p = Bi_pack.data() + jr * kc;
                    for (std::size_t kk = 0; kk < kc; ++kk) {
                        for (std::size_t c = 0; c < cur_nr; ++c) {
                            Complex b_val = B[(pc + kk) + (jc + jr + c) * ldb];
                            br_p[kk * nr + c] = b_val.real();
                            bi_p[kk * nr + c] = b_val.imag();
                        }
                        for (std::size_t c = cur_nr; c < nr; ++c) {
                            br_p[kk * nr + c] = 0.0;
                            bi_p[kk * nr + c] = 0.0;
                        }
                    }
                }

                for (std::size_t ic = 0; ic < m; ic += mc_block) {
                    std::size_t mc = std::min(mc_block, m - ic);

                    // Pack A real and imaginary blocks ONCE per tile
                    for (std::size_t ii = 0; ii < mc; ii += mr) {
                        std::size_t cur_mr = std::min(mr, mc - ii);
                        double *ar_p = Ar_pack.data() + ii * kc;
                        double *ai_p = Ai_pack.data() + ii * kc;
                        for (std::size_t kk = 0; kk < kc; ++kk) {
                            const Complex *a_col = A + (ic + ii) + (pc + kk) * lda;
                            for (std::size_t r = 0; r < cur_mr; ++r) {
                                ar_p[kk * mr + r] = a_col[r].real();
                                ai_p[kk * mr + r] = a_col[r].imag();
                            }
                            for (std::size_t r = cur_mr; r < mr; ++r) {
                                ar_p[kk * mr + r] = 0.0;
                                ai_p[kk * mr + r] = 0.0;
                            }
                        }
                    }

                    for (std::size_t jr = 0; jr < nc; jr += nr) {
                        std::size_t cur_nr = std::min(nr, nc - jr);
                        const double *br_p_tile = Br_pack.data() + jr * kc;
                        const double *bi_p_tile = Bi_pack.data() + jr * kc;

                        for (std::size_t ir = 0; ir < mc; ir += mr) {
                            std::size_t cur_mr = std::min(mr, mc - ir);
                            const double *ar_p = Ar_pack.data() + ir * kc;
                            const double *ai_p = Ai_pack.data() + ir * kc;
                            Complex *c_ptr = C + (ic + ir) + (jc + jr) * ldc;

                            if (cur_mr == mr && cur_nr == nr) {
                                // 4M SIMD accumulators: Real part (cr) and Imag part (ci)
                                auto cr00 = hn::Zero(d), cr01 = hn::Zero(d), cr02 = hn::Zero(d), cr03 = hn::Zero(d), cr04 = hn::Zero(d), cr05 = hn::Zero(d);
                                auto cr10 = hn::Zero(d), cr11 = hn::Zero(d), cr12 = hn::Zero(d), cr13 = hn::Zero(d), cr14 = hn::Zero(d), cr15 = hn::Zero(d);
                                auto ci00 = hn::Zero(d), ci01 = hn::Zero(d), ci02 = hn::Zero(d), ci03 = hn::Zero(d), ci04 = hn::Zero(d), ci05 = hn::Zero(d);
                                auto ci10 = hn::Zero(d), ci11 = hn::Zero(d), ci12 = hn::Zero(d), ci13 = hn::Zero(d), ci14 = hn::Zero(d), ci15 = hn::Zero(d);

                                for (std::size_t kk = 0; kk < kc; ++kk) {
                                    auto ar0 = hn::LoadU(d, ar_p + kk * mr + 0 * N);
                                    auto ar1 = hn::LoadU(d, ar_p + kk * mr + 1 * N);
                                    auto ai0 = hn::LoadU(d, ai_p + kk * mr + 0 * N);
                                    auto ai1 = hn::LoadU(d, ai_p + kk * mr + 1 * N);

                                    const double *br_k = br_p_tile + kk * nr;
                                    const double *bi_k = bi_p_tile + kk * nr;

                                    #define KERNEL_COL(col_idx, cr_acc0, cr_acc1, ci_acc0, ci_acc1) \
                                    { \
                                        auto br = hn::Set(d, br_k[col_idx]); \
                                        auto bi = hn::Set(d, bi_k[col_idx]); \
                                        cr_acc0 = hn::MulAdd(ar0, br, cr_acc0); \
                                        cr_acc0 = hn::NegMulAdd(ai0, bi, cr_acc0); \
                                        cr_acc1 = hn::MulAdd(ar1, br, cr_acc1); \
                                        cr_acc1 = hn::NegMulAdd(ai1, bi, cr_acc1); \
                                        ci_acc0 = hn::MulAdd(ar0, bi, ci_acc0); \
                                        ci_acc0 = hn::MulAdd(ai0, br, ci_acc0); \
                                        ci_acc1 = hn::MulAdd(ar1, bi, ci_acc1); \
                                        ci_acc1 = hn::MulAdd(ai1, br, ci_acc1); \
                                    }

                                    KERNEL_COL(0, cr00, cr10, ci00, ci10);
                                    KERNEL_COL(1, cr01, cr11, ci01, ci11);
                                    KERNEL_COL(2, cr02, cr12, ci02, ci12);
                                    KERNEL_COL(3, cr03, cr13, ci03, ci13);
                                    KERNEL_COL(4, cr04, cr14, ci04, ci14);
                                    KERNEL_COL(5, cr05, cr15, ci05, ci15);
                                    #undef KERNEL_COL
                                }

                                auto v_alpha_r = hn::Set(d, alpha.real());
                                auto v_alpha_i = hn::Set(d, alpha.imag());
                                auto v_beta_r  = hn::Set(d, current_beta.real());
                                auto v_beta_i  = hn::Set(d, current_beta.imag());

                                #define STORE_COMPLEX_COL(col_idx, cr0, cr1, ci0, ci1) \
                                { \
                                    Complex *col_c = c_ptr + col_idx * ldc; \
                                    auto yr0 = hn::Mul(cr0, v_alpha_r); \
                                    yr0 = hn::NegMulAdd(ci0, v_alpha_i, yr0); \
                                    auto yr1 = hn::Mul(cr1, v_alpha_r); \
                                    yr1 = hn::NegMulAdd(ci1, v_alpha_i, yr1); \
                                    auto yi0 = hn::Mul(cr0, v_alpha_i); \
                                    yi0 = hn::MulAdd(ci0, v_alpha_r, yi0); \
                                    auto yi1 = hn::Mul(cr1, v_alpha_i); \
                                    yi1 = hn::MulAdd(ci1, v_alpha_r, yi1); \
                                    alignas(64) double buf_r0[16], buf_r1[16], buf_i0[16], buf_i1[16]; \
                                    hn::StoreU(yr0, d, buf_r0); \
                                    hn::StoreU(yr1, d, buf_r1); \
                                    hn::StoreU(yi0, d, buf_i0); \
                                    hn::StoreU(yi1, d, buf_i1); \
                                    for (std::size_t r = 0; r < N; ++r) { \
                                        Complex c_val = Complex(buf_r0[r], buf_i0[r]); \
                                        if (current_beta != Complex(0.0, 0.0)) { \
                                            c_val += col_c[r + 0 * N] * current_beta; \
                                        } \
                                        col_c[r + 0 * N] = c_val; \
                                    } \
                                    for (std::size_t r = 0; r < N; ++r) { \
                                        Complex c_val = Complex(buf_r1[r], buf_i1[r]); \
                                        if (current_beta != Complex(0.0, 0.0)) { \
                                            c_val += col_c[r + 1 * N] * current_beta; \
                                        } \
                                        col_c[r + 1 * N] = c_val; \
                                    } \
                                }

                                STORE_COMPLEX_COL(0, cr00, cr10, ci00, ci10);
                                STORE_COMPLEX_COL(1, cr01, cr11, ci01, ci11);
                                STORE_COMPLEX_COL(2, cr02, cr12, ci02, ci12);
                                STORE_COMPLEX_COL(3, cr03, cr13, ci03, ci13);
                                STORE_COMPLEX_COL(4, cr04, cr14, ci04, ci14);
                                STORE_COMPLEX_COL(5, cr05, cr15, ci05, ci15);
                                #undef STORE_COMPLEX_COL
                            } else {
                                // Tail scalar edge kernel
                                for (std::size_t c = 0; c < cur_nr; ++c) {
                                    Complex *col_c = c_ptr + c * ldc;
                                    const double *br_col = br_p_tile + c;
                                    const double *bi_col = bi_p_tile + c;
                                    for (std::size_t r = 0; r < cur_mr; ++r) {
                                        Complex acc(0.0, 0.0);
                                        for (std::size_t kk = 0; kk < kc; ++kk) {
                                            Complex a_val(ar_p[kk * mr + r], ai_p[kk * mr + r]);
                                            Complex b_val(br_col[kk * nr], bi_col[kk * nr]);
                                            acc += a_val * b_val;
                                        }
                                        if (current_beta == Complex(0.0, 0.0)) {
                                            col_c[r] = alpha * acc;
                                        } else {
                                            col_c[r] = current_beta * col_c[r] + alpha * acc;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    });
}

void GemvDoubleKernel(std::size_t m, std::size_t n,
                      double alpha, const double *A, std::size_t lda,
                      const double *x, std::size_t incx,
                      double beta, double *y, std::size_t incy)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);

    if (beta == 0.0) {
        for (std::size_t i = 0; i < m; ++i) y[i * incy] = 0.0;
    } else if (beta != 1.0) {
        for (std::size_t i = 0; i < m; ++i) y[i * incy] *= beta;
    }

    if (alpha == 0.0 || n == 0) return;

    if (incy == 1) {
        for (std::size_t j = 0; j < n; ++j) {
            const double xj = alpha * x[j * incx];
            if (xj == 0.0) continue;
            const double *aj = A + j * lda;
            auto v_x = hn::Set(d, xj);

            std::size_t i = 0;
            for (; i + N <= m; i += N) {
                auto v_y = hn::MulAdd(hn::LoadU(d, aj + i), v_x, hn::LoadU(d, y + i));
                hn::StoreU(v_y, d, y + i);
            }
            for (; i < m; ++i) y[i] += aj[i] * xj;
        }
    } else {
        for (std::size_t j = 0; j < n; ++j) {
            const double xj = alpha * x[j * incx];
            if (xj == 0.0) continue;
            const double *aj = A + j * lda;
            for (std::size_t i = 0; i < m; ++i) y[i * incy] += aj[i] * xj;
        }
    }
}

void GerDoubleKernel(std::size_t m, std::size_t n,
                     double alpha, const double *x, std::size_t incx,
                     const double *y, std::size_t incy,
                     double *A, std::size_t lda)
{
    const hn::ScalableTag<double> d;
    const std::size_t N = hn::Lanes(d);

    if (alpha == 0.0) return;

    for (std::size_t j = 0; j < n; ++j) {
        const double yj = alpha * y[j * incy];
        if (yj == 0.0) continue;
        double *aj = A + j * lda;
        auto v_y = hn::Set(d, yj);

        if (incx == 1) {
            std::size_t i = 0;
            for (; i + N <= m; i += N) {
                auto v_a = hn::MulAdd(hn::LoadU(d, x + i), v_y, hn::LoadU(d, aj + i));
                hn::StoreU(v_a, d, aj + i);
            }
            for (; i < m; ++i) aj[i] += x[i] * yj;
        } else {
            for (std::size_t i = 0; i < m; ++i) aj[i] += x[i * incx] * yj;
        }
    }
}

} // namespace HWY_NAMESPACE
} // namespace numkit::ops
HWY_AFTER_NAMESPACE();

#if HWY_ONCE
namespace numkit::ops {

HWY_EXPORT(GemmDoubleKernel);
HWY_EXPORT(GemmComplexKernel);
HWY_EXPORT(GemvDoubleKernel);
HWY_EXPORT(GerDoubleKernel);

void gemm(std::size_t m, std::size_t n, std::size_t k,
          double alpha, const double *A, std::size_t lda,
          const double *B, std::size_t ldb,
          double beta, double *C, std::size_t ldc)
{
    HWY_DYNAMIC_DISPATCH(GemmDoubleKernel)(m, n, k, alpha, A, lda, B, ldb, beta, C, ldc);
}

void gemm(std::size_t m, std::size_t n, std::size_t k,
          std::complex<double> alpha, const std::complex<double> *A, std::size_t lda,
          const std::complex<double> *B, std::size_t ldb,
          std::complex<double> beta, std::complex<double> *C, std::size_t ldc)
{
    HWY_DYNAMIC_DISPATCH(GemmComplexKernel)(m, n, k, alpha, A, lda, B, ldb, beta, C, ldc);
}

void gemv(std::size_t m, std::size_t n,
          double alpha, const double *A, std::size_t lda,
          const double *x, std::size_t incx,
          double beta, double *y, std::size_t incy)
{
    HWY_DYNAMIC_DISPATCH(GemvDoubleKernel)(m, n, alpha, A, lda, x, incx, beta, y, incy);
}

void ger(std::size_t m, std::size_t n,
         double alpha, const double *x, std::size_t incx,
         const double *y, std::size_t incy,
         double *A, std::size_t lda)
{
    HWY_DYNAMIC_DISPATCH(GerDoubleKernel)(m, n, alpha, x, incx, y, incy, A, lda);
}

template <typename T> struct is_complex_type : std::false_type {};
template <typename U> struct is_complex_type<std::complex<U>> : std::true_type {};
template <typename T> inline constexpr bool is_complex_type_v = is_complex_type<T>::value;

template <typename T>
void trsm_generic(MatrixSide side, MatrixUplo uplo, MatrixTranspose trans, MatrixDiag diag,
                  std::size_t m, std::size_t n,
                  T alpha, const T *A, std::size_t lda,
                  T *B, std::size_t ldb)
{
    if (m == 0 || n == 0) return;

    if (alpha != T(1)) {
        for (std::size_t j = 0; j < n; ++j) {
            for (std::size_t i = 0; i < m; ++i) {
                B[i + j * ldb] *= alpha;
            }
        }
    }

    const bool is_unit = (diag == MatrixDiag::Unit);
    const bool is_conj = (trans == MatrixTranspose::ConjTrans);

    auto get_a = [&](std::size_t r, std::size_t c) -> T {
        T val = (trans == MatrixTranspose::NoTrans) ? A[r + c * lda] : A[c + r * lda];
        if constexpr (is_complex_type_v<T>) {
            if (is_conj && trans != MatrixTranspose::NoTrans) return std::conj(val);
        }
        return val;
    };

    if (side == MatrixSide::Left) {
        // op(A) * X = B, A is m x m
        if (uplo == MatrixUplo::Lower && trans == MatrixTranspose::NoTrans) {
            for (std::size_t j = 0; j < n; ++j) {
                for (std::size_t k = 0; k < m; ++k) {
                    if (!is_unit) B[k + j * ldb] /= get_a(k, k);
                    const T xkj = B[k + j * ldb];
                    for (std::size_t i = k + 1; i < m; ++i) {
                        B[i + j * ldb] -= get_a(i, k) * xkj;
                    }
                }
            }
        } else if (uplo == MatrixUplo::Lower && trans != MatrixTranspose::NoTrans) {
            for (std::size_t j = 0; j < n; ++j) {
                for (std::intptr_t k = static_cast<std::intptr_t>(m) - 1; k >= 0; --k) {
                    if (!is_unit) B[k + j * ldb] /= get_a(k, k);
                    const T xkj = B[k + j * ldb];
                    for (std::intptr_t i = k - 1; i >= 0; --i) {
                        B[i + j * ldb] -= get_a(i, k) * xkj;
                    }
                }
            }
        } else if (uplo == MatrixUplo::Upper && trans == MatrixTranspose::NoTrans) {
            for (std::size_t j = 0; j < n; ++j) {
                for (std::intptr_t k = static_cast<std::intptr_t>(m) - 1; k >= 0; --k) {
                    if (!is_unit) B[k + j * ldb] /= get_a(k, k);
                    const T xkj = B[k + j * ldb];
                    for (std::intptr_t i = k - 1; i >= 0; --i) {
                        B[i + j * ldb] -= get_a(i, k) * xkj;
                    }
                }
            }
        } else { // Upper + Trans / ConjTrans
            for (std::size_t j = 0; j < n; ++j) {
                for (std::size_t k = 0; k < m; ++k) {
                    if (!is_unit) B[k + j * ldb] /= get_a(k, k);
                    const T xkj = B[k + j * ldb];
                    for (std::size_t i = k + 1; i < m; ++i) {
                        B[i + j * ldb] -= get_a(i, k) * xkj;
                    }
                }
            }
        }
    } else {
        // X * op(A) = B, A is n x n
        if (uplo == MatrixUplo::Lower && trans == MatrixTranspose::NoTrans) {
            for (std::intptr_t k = static_cast<std::intptr_t>(n) - 1; k >= 0; --k) {
                if (!is_unit) {
                    T akk = get_a(k, k);
                    for (std::size_t i = 0; i < m; ++i) B[i + k * ldb] /= akk;
                }
                for (std::intptr_t j = k - 1; j >= 0; --j) {
                    T akj = get_a(k, j);
                    for (std::size_t i = 0; i < m; ++i) {
                        B[i + j * ldb] -= B[i + k * ldb] * akj;
                    }
                }
            }
        } else if (uplo == MatrixUplo::Lower && trans != MatrixTranspose::NoTrans) {
            for (std::size_t k = 0; k < n; ++k) {
                if (!is_unit) {
                    T akk = get_a(k, k);
                    for (std::size_t i = 0; i < m; ++i) B[i + k * ldb] /= akk;
                }
                for (std::size_t j = k + 1; j < n; ++j) {
                    T akj = get_a(k, j);
                    for (std::size_t i = 0; i < m; ++i) {
                        B[i + j * ldb] -= B[i + k * ldb] * akj;
                    }
                }
            }
        } else if (uplo == MatrixUplo::Upper && trans == MatrixTranspose::NoTrans) {
            for (std::size_t k = 0; k < n; ++k) {
                if (!is_unit) {
                    T akk = get_a(k, k);
                    for (std::size_t i = 0; i < m; ++i) B[i + k * ldb] /= akk;
                }
                for (std::size_t j = k + 1; j < n; ++j) {
                    T akj = get_a(k, j);
                    for (std::size_t i = 0; i < m; ++i) {
                        B[i + j * ldb] -= B[i + k * ldb] * akj;
                    }
                }
            }
        } else { // Upper + Trans / ConjTrans
            for (std::intptr_t k = static_cast<std::intptr_t>(n) - 1; k >= 0; --k) {
                if (!is_unit) {
                    T akk = get_a(k, k);
                    for (std::size_t i = 0; i < m; ++i) B[i + k * ldb] /= akk;
                }
                for (std::intptr_t j = k - 1; j >= 0; --j) {
                    T akj = get_a(k, j);
                    for (std::size_t i = 0; i < m; ++i) {
                        B[i + j * ldb] -= B[i + k * ldb] * akj;
                    }
                }
            }
        }
    }
}

void trsm(MatrixSide side, MatrixUplo uplo, MatrixTranspose trans, MatrixDiag diag,
          std::size_t m, std::size_t n,
          double alpha, const double *A, std::size_t lda,
          double *B, std::size_t ldb)
{
    trsm_generic(side, uplo, trans, diag, m, n, alpha, A, lda, B, ldb);
}

void trsm(MatrixSide side, MatrixUplo uplo, MatrixTranspose trans, MatrixDiag diag,
          std::size_t m, std::size_t n,
          std::complex<double> alpha, const std::complex<double> *A, std::size_t lda,
          std::complex<double> *B, std::size_t ldb)
{
    trsm_generic(side, uplo, trans, diag, m, n, alpha, A, lda, B, ldb);
}

template <typename T>
void syrk_generic(MatrixUplo uplo, MatrixTranspose trans,
                  std::size_t n, std::size_t k,
                  T alpha, const T *A, std::size_t lda,
                  T beta, T *C, std::size_t ldc)
{
    if (n == 0) return;

    const std::size_t total_flops = 2 * n * n * k;
    constexpr std::size_t kParallelFlopThreshold = 64'000;
    const std::size_t p_thresh = (total_flops >= kParallelFlopThreshold) ? std::size_t{1} : n + 1;

    const bool is_trans = (trans != MatrixTranspose::NoTrans);
    const bool is_conj  = (trans == MatrixTranspose::ConjTrans);

    numkit::detail::parallel_for(n, p_thresh, [=](std::size_t jc_start, std::size_t jc_end) {
        for (std::size_t j = jc_start; j < jc_end; ++j) {
            std::size_t i_start = (uplo == MatrixUplo::Lower) ? j : 0;
            std::size_t i_end   = (uplo == MatrixUplo::Lower) ? n : j + 1;

            for (std::size_t i = i_start; i < i_end; ++i) {
                if (beta == T(0)) C[i + j * ldc] = T(0);
                else if (beta != T(1)) C[i + j * ldc] *= beta;
            }

            if (k == 0 || alpha == T(0)) continue;

            for (std::size_t l = 0; l < k; ++l) {
                T b_val = is_trans ? A[l + j * lda] : A[j + l * lda];
                if constexpr (is_complex_type_v<T>) {
                    if (!is_trans) {
                        b_val = std::conj(b_val);
                    }
                }
                const T alpha_b = alpha * b_val;

                for (std::size_t i = i_start; i < i_end; ++i) {
                    T a_val = is_trans ? A[l + i * lda] : A[i + l * lda];
                    if constexpr (is_complex_type_v<T>) {
                        if (is_conj) {
                            a_val = std::conj(a_val);
                        }
                    }
                    C[i + j * ldc] += a_val * alpha_b;
                }
            }
        }
    });
}

void syrk(MatrixUplo uplo, MatrixTranspose trans,
          std::size_t n, std::size_t k,
          double alpha, const double *A, std::size_t lda,
          double beta, double *C, std::size_t ldc)
{
    syrk_generic(uplo, trans, n, k, alpha, A, lda, beta, C, ldc);
}

void syrk(MatrixUplo uplo, MatrixTranspose trans,
          std::size_t n, std::size_t k,
          std::complex<double> alpha, const std::complex<double> *A, std::size_t lda,
          std::complex<double> beta, std::complex<double> *C, std::size_t ldc)
{
    syrk_generic(uplo, trans, n, k, alpha, A, lda, beta, C, ldc);
}

} // namespace numkit::ops
#endif
