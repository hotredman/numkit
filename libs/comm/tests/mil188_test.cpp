// libs/comm/tests/mil188_test.cpp
//
// Regression guard for mil188qammod / mil188qamdemod
// (MIL-STD-188-110 QAM, currently M=16 only).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class Mil188Test : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(Mil188Test, KnownConstellationPoints16)
{
    // Bit-equal with MATLAB R2025b's mil188qammod((0:15)', 16).
    eval("y = mil188qammod((0:15)', 16);");
    // MATLAB uses 6-digit-rounded spec values (NOT full cos(30°)),
    // so we exact-match those.
    EXPECT_DOUBLE_EQ(evalScalar("real(y(1))"),  0.866025);
    EXPECT_DOUBLE_EQ(evalScalar("imag(y(1))"),  0.5);
    EXPECT_DOUBLE_EQ(evalScalar("real(y(3))"),  1.0);
    EXPECT_DOUBLE_EQ(evalScalar("imag(y(3))"),  0.0);
    // Inner ring point at idx 3 (1-based 4):
    EXPECT_DOUBLE_EQ(evalScalar("real(y(4))"),  0.258819);
    EXPECT_DOUBLE_EQ(evalScalar("imag(y(4))"),  0.258819);
    // y(15) at idx 14 = -1+0i (outer ring at 180°):
    EXPECT_DOUBLE_EQ(evalScalar("real(y(15))"), -1.0);
    EXPECT_DOUBLE_EQ(evalScalar("imag(y(15))"),  0.0);
}

TEST_F(Mil188Test, RoundTrip16)
{
    eval("x = (0:15)';"
         "y = mil188qammod(x, 16);"
         "z = mil188qamdemod(y, 16);"
         "match = isequal(x, z);");
    EXPECT_DOUBLE_EQ(evalScalar("match"), 1.0);
}

TEST_F(Mil188Test, NearestNeighborDemodWithNoise)
{
    eval("x = (0:15)';"
         "y = mil188qammod(x, 16);"
         "noisy = y + 0.05*(1+1i);"
         "z = mil188qamdemod(noisy, 16);"
         "match = isequal(x, z);");
    EXPECT_DOUBLE_EQ(evalScalar("match"), 1.0);
}

TEST_F(Mil188Test, OuterRingNearUnitRadius)
{
    // Indices on the outer ring have abs(y) ≈ 1 (within 6-digit
    // round-off of MATLAB's spec values: 0.866025^2 + 0.5^2 differs
    // from 1 by ~5e-7).
    eval("y = mil188qammod((0:15)', 16);"
         "outer_idx = [1 2 3 5 6 7 9 10 11 13 14 15];"
         "outer = y(outer_idx);"
         "ok = all(abs(abs(outer) - 1) < 1e-6);");
    EXPECT_DOUBLE_EQ(evalScalar("ok"), 1.0);
}

TEST_F(Mil188Test, InnerRingFixedRadius)
{
    // Inner-ring points (idx 3, 7, 11, 15 in 0-based = MATLAB 4,8,12,16).
    eval("y = mil188qammod((0:15)', 16);"
         "r = abs(y(4));"
         "ok = abs(abs(y(8)) - r) < 1e-12 && abs(abs(y(12)) - r) < 1e-12 && abs(abs(y(16)) - r) < 1e-12;");
    EXPECT_DOUBLE_EQ(evalScalar("ok"), 1.0);
}

TEST_F(Mil188Test, ShapePreserved)
{
    eval("y = mil188qammod([0 1; 2 3], 16);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(y, 2)")), 2);
}

TEST_F(Mil188Test, KnownConstellationPoints32)
{
    eval("y = mil188qammod((0:31)', 32);");
    EXPECT_DOUBLE_EQ(evalScalar("real(y(1))"),  0.86638);
    EXPECT_DOUBLE_EQ(evalScalar("imag(y(1))"),  0.499386);
    EXPECT_DOUBLE_EQ(evalScalar("real(y(2))"),  0.984849);
    EXPECT_DOUBLE_EQ(evalScalar("imag(y(2))"),  0.173415);
    EXPECT_DOUBLE_EQ(evalScalar("real(y(8))"),  0.173415);
    EXPECT_DOUBLE_EQ(evalScalar("imag(y(8))"),  0.173415);
    EXPECT_DOUBLE_EQ(evalScalar("real(y(9))"), -0.86638);
}

TEST_F(Mil188Test, RoundTrip32)
{
    eval("x = (0:31)';"
         "y = mil188qammod(x, 32);"
         "z = mil188qamdemod(y, 32);"
         "match = isequal(x, z);");
    EXPECT_DOUBLE_EQ(evalScalar("match"), 1.0);
}

TEST_F(Mil188Test, NearestNeighborDemod32WithNoise)
{
    eval("x = (0:31)';"
         "y = mil188qammod(x, 32);"
         "z = mil188qamdemod(y + 0.02*(1+1i), 32);"
         "match = isequal(x, z);");
    EXPECT_DOUBLE_EQ(evalScalar("match"), 1.0);
}

TEST_F(Mil188Test, RejectsUnsupportedM)
{
    bool threw = false;
    try { eval("mil188qammod(0, 64);"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}

TEST_F(Mil188Test, RejectsOutOfRangeIndex)
{
    bool threw = false;
    try { eval("mil188qammod(99, 16);"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}
