// libs/comm/tests/genqam_test.cpp
//
// Regression guard for genqammod / genqamdemod (generic
// constellation modulation/demodulation).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class GenqamTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(GenqamTest, ModEightPSK)
{
    eval("M = 8; const = exp(1i*2*pi*(0:M-1)/M);"
         "y = genqammod((0:M-1)', const);");
    EXPECT_EQ(static_cast<int>(evalScalar("numel(y)")), 8);
    EXPECT_NEAR(evalScalar("real(y(1))"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(y(1))"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("real(y(3))"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("imag(y(3))"), 1.0, 1e-12);
    EXPECT_NEAR(evalScalar("real(y(5))"), -1.0, 1e-12);
}

TEST_F(GenqamTest, RoundTripIntegerInput)
{
    eval("const = exp(1i*2*pi*(0:7)/8);"
         "x = (0:7)';"
         "y = genqammod(x, const);"
         "z = genqamdemod(y, const);"
         "match = isequal(x, z);");
    EXPECT_DOUBLE_EQ(evalScalar("match"), 1.0);
}

TEST_F(GenqamTest, RoundTripCustomFourPoint)
{
    eval("const = [1+0i, 0+1i, -1+0i, 0-1i];"
         "x = [0 1 2 3 0 1]';"
         "y = genqammod(x, const);"
         "z = genqamdemod(y, const);"
         "match = isequal(x, z);");
    EXPECT_DOUBLE_EQ(evalScalar("match"), 1.0);
}

TEST_F(GenqamTest, RealPAMConstellation)
{
    eval("const = [-3 -1 1 3];"
         "y = genqammod((0:3)', const);");
    EXPECT_DOUBLE_EQ(evalScalar("y(1)"), -3.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(2)"), -1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(3)"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("y(4)"),  3.0);
}

TEST_F(GenqamTest, DemodNearestNeighbor)
{
    // Add small noise; demod should still pick correct index.
    eval("const = exp(1i*2*pi*(0:7)/8);"
         "x = (0:7)';"
         "y = genqammod(x, const);"
         "noisy = y + 0.05*(1 + 1i);"
         "z = genqamdemod(noisy, const);"
         "match = isequal(x, z);");
    EXPECT_DOUBLE_EQ(evalScalar("match"), 1.0);
}

TEST_F(GenqamTest, MatrixShapePreserved)
{
    eval("const = exp(1i*2*pi*(0:7)/8);"
         "y = genqammod([0 1; 2 3], const);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 2)")), 2);
}

TEST_F(GenqamTest, RejectsOutOfRangeIndex)
{
    bool threw = false;
    try {
        eval("const = exp(1i*2*pi*(0:3)/4);"
             "genqammod(5, const);");  // index 5 with M=4 → out of range
    } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

TEST_F(GenqamTest, DemodRealConstellationFromComplexInput)
{
    // Real PAM, but pass complex y vector with tiny imag to demod.
    eval("const = [-3 -1 1 3];"
         "y = [-3 + 0.01i; -1 - 0.02i; 1 + 0i; 3];"
         "z = genqamdemod(y, const);");
    EXPECT_DOUBLE_EQ(evalScalar("z(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("z(2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("z(3)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("z(4)"), 3.0);
}
