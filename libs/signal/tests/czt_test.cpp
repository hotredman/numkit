// libs/signal/tests/czt_test.cpp
//
// gtest coverage for czt — discrete chirp Z-transform via Bluestein.
// Pins (a) the FFT-equivalent default-args path, (b) the m-override
// path matching fft(x, m), and (c) the full 4-arg general chirp.
// MATLAB fingerprints captured from R2025b probe.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class CztTest : public ::testing::Test
{
public:
    numkit::StdEngine engine;
    void   SetUp() override { engine.eval("import compat.*;"); }
    double eval_scalar(const std::string &c) { return engine.eval(c).toScalar(); }
};

// czt(x) with default w = exp(-2πj/N), a = 1, m = N must match
// fft(x) bit-for-bit up to Bluestein chirp-arithmetic noise.
TEST_F(CztTest, DefaultArgsMatchesFft)
{
    engine.eval("x = 1:8;");
    engine.eval("y = czt(x); ref = fft(x);");
    engine.eval("err = max(abs(y - ref));");
    EXPECT_LT(eval_scalar("err"), 1e-12);
    EXPECT_NEAR(eval_scalar("real(y(1))"), 36.0, 1e-12);  // sum(1..8)
}

// czt(x, m) — same as fft(x, m). m > length(x) → zero-pad.
TEST_F(CztTest, MOverrideMatchesFftM)
{
    engine.eval("x = 1:8;");
    engine.eval("y = czt(x, 16); ref = fft(x, 16);");
    EXPECT_LT(eval_scalar("max(abs(y - ref))"), 1e-12);
    EXPECT_DOUBLE_EQ(eval_scalar("length(y)"), 16.0);
}

// czt(x, m, w, a) — full general chirp. Probed against MATLAB R2025b
// with x=1:8, m=10, w=exp(-2πj/16), a=exp(πj/8) (spiral starting
// off-unit-circle with 1/16-cycle step).
TEST_F(CztTest, GeneralChirp)
{
    engine.eval("x = 1:8; w = exp(-1j*2*pi*1/16); a = exp(1j*pi/8);");
    engine.eval("y = czt(x, 10, w, a);");
    EXPECT_DOUBLE_EQ(eval_scalar("length(y)"), 10.0);
    EXPECT_NEAR(eval_scalar("real(y(1))"),  -8.13707,  1e-4);
    EXPECT_NEAR(eval_scalar("imag(y(1))"), -25.1367,   1e-4);
    EXPECT_NEAR(eval_scalar("real(y(5))"),   4.27677,  1e-4);
    EXPECT_NEAR(eval_scalar("imag(y(5))"),  -3.34089,  1e-4);
    EXPECT_NEAR(eval_scalar("real(y(10))"), -4.0,      1e-4);
    EXPECT_NEAR(eval_scalar("imag(y(10))"), -1.65685,  1e-4);
}

// Empty input — MATLAB returns empty without error.
TEST_F(CztTest, EmptyInput)
{
    engine.eval("y = czt([]);");
    EXPECT_DOUBLE_EQ(eval_scalar("numel(y)"), 0.0);
}

// m must be a positive integer — m = 0 throws.
TEST_F(CztTest, ZeroMThrows)
{
    EXPECT_THROW(engine.eval("czt([1 2 3], 0);"), std::exception);
}

// Column-vector input — output preserves column orientation.
TEST_F(CztTest, ColumnVectorPreservesShape)
{
    engine.eval("x = (1:8)';");
    engine.eval("y = czt(x);");
    EXPECT_DOUBLE_EQ(eval_scalar("size(y, 1)"), 8.0);
    EXPECT_DOUBLE_EQ(eval_scalar("size(y, 2)"), 1.0);
}
