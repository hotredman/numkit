// libs/builtin/tests/sprintf_complex_test.cpp
//
// Regression guard for bugs/builtin/sprintf-complex.md: sprintf/fprintf used to
// throw on a complex argument to a numeric conversion. MATLAB R2025b uses the
// REAL part (imaginary discarded). Expected strings are bit-exact MATLAB R2025b.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class SprintfComplexTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    std::string evalStr(const std::string &c) { return eval(c).toString(); }
};

// Scalar complex via %g -> real part.
TEST_F(SprintfComplexTest, ScalarG)
{
    EXPECT_EQ(evalStr("sprintf('%g', 1+2i)"), "1");
    EXPECT_EQ(evalStr("sprintf('%.2f', 3.5-1.5i)"), "3.50");
}

// Vector complex via %d -> real parts, format cycles.
TEST_F(SprintfComplexTest, VectorD)
{
    EXPECT_EQ(evalStr("sprintf('%d ', [1+2i 3+4i])"), "1 3 ");
}

// Mixed zero-imag + real in one vector.
TEST_F(SprintfComplexTest, MixedZeroImag)
{
    EXPECT_EQ(evalStr("sprintf('%g ', [1.5+0i 2.5])"), "1.5 2.5 ");
}

// complex(x,0) (explicitly complex storage, zero imaginary).
TEST_F(SprintfComplexTest, ExplicitComplexZeroImag)
{
    EXPECT_EQ(evalStr("sprintf('%d', complex(7,0))"), "7");
}

// Real arguments still format exactly as before (regression).
TEST_F(SprintfComplexTest, RealUnaffected)
{
    EXPECT_EQ(evalStr("sprintf('%g', 1000000)"), "1e+06");
    EXPECT_EQ(evalStr("sprintf('%d ', [1 2 3])"), "1 2 3 ");
    EXPECT_EQ(evalStr("sprintf('%.3f', pi)"), "3.142");
}
