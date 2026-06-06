// libs/comm/tests/gen2par_test.cpp
//
// Regression guard for gen2par (Error Correction Codes). Reference values
// from the MATLAB R2025b probe:
//   gen2par([P|I_4]) of the Hamming(7,4) generator -> the parity matrix;
//   gen2par([I_3|P]) -> [P' | I_2]; the map is an involution.

#include <numkit/comm/coding/blockcoding.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class Gen2parTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// [P | I_4] generator (Hamming(7,4)) -> [I_3 | P'] parity-check matrix.
TEST_F(Gen2parTest, GeneratorToParityLastIdentity)
{
    eval("G = [1 1 0 1 0 0 0; 0 1 1 0 1 0 0; 1 1 1 0 0 1 0; 1 0 1 0 0 0 1];"
         " H = gen2par(G);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(H,1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(H,2)")), 7);
    EXPECT_DOUBLE_EQ(evalScalar(
        "isequal(H, [1 0 0 1 0 1 1; 0 1 0 1 1 1 0; 0 0 1 0 1 1 1])"), 1.0);
}

// [I_3 | P] generator -> [P' | I_2].
TEST_F(Gen2parTest, GeneratorToParityFirstIdentity)
{
    eval("G = [1 0 0 1 1; 0 1 0 0 1; 0 0 1 1 0]; H = gen2par(G);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(H,1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(H,2)")), 5);
    EXPECT_DOUBLE_EQ(evalScalar("isequal(H, [1 0 1 1 0; 1 1 0 0 1])"), 1.0);
}

// gen2par is an involution: gen2par(gen2par(G)) == G.
TEST_F(Gen2parTest, Involution)
{
    eval("G = [1 0 0 1 1; 0 1 0 0 1; 0 0 1 1 0];");
    EXPECT_DOUBLE_EQ(evalScalar("isequal(gen2par(gen2par(G)), G)"), 1.0);
}

TEST_F(Gen2parTest, Errors)
{
    // r >= c (too few columns) -> error
    EXPECT_ANY_THROW(eval("gen2par([1 0; 0 1; 1 1]);"));
    // non-systematic (no identity block) -> error
    EXPECT_ANY_THROW(eval("gen2par([1 1 0 1; 0 1 1 1]);"));
}

// Direct C++ API.
TEST_F(Gen2parTest, PublicApi)
{
    eval("G = [1 1 0 1 0 0 0; 0 1 1 0 1 0 0; 1 1 1 0 0 1 0; 1 0 1 0 0 0 1];");
    Value h = comm::gen2par(*engine.getVariable("G"), engine.resource());
    ASSERT_EQ(h.dims().rows(), 3u);
    ASSERT_EQ(h.dims().cols(), 7u);
    // H(1,4) 1-based = col-major index 3*3 + 0 = 9 -> 1.0
    EXPECT_DOUBLE_EQ(h.doubleData()[3 * 3 + 0], 1.0);
    // H(1,1) = 1 (identity block)
    EXPECT_DOUBLE_EQ(h.doubleData()[0], 1.0);
}
