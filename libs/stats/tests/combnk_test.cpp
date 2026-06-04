// libs/stats/tests/combnk_test.cpp
//
// Regression guard for bugs/stats/combnk-scalar.md (fixed): a SCALAR first
// arg is the 1-element set {v} (NOT 1:v), and K > N yields an empty 0xK
// result instead of an error. Expected values from MATLAB R2025b.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class CombnkTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(CombnkTest, ScalarIsOneElementSet)
{
    // combnk(5,2): set {5}, choose 2 -> empty 0x2 (MATLAB), not C(5,2)=10.
    EXPECT_EQ(static_cast<int>(evalScalar("size(combnk(5,2),1)")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("size(combnk(5,2),2)")), 2);
}

TEST_F(CombnkTest, ScalarChooseOne)
{
    // combnk(5,1): set {5}, choose 1 -> [5] (1x1).
    EXPECT_EQ(static_cast<int>(evalScalar("numel(combnk(5,1))")), 1);
    EXPECT_NEAR(evalScalar("combnk(5,1)"), 5.0, 1e-12);
}

TEST_F(CombnkTest, KGreaterThanNIsEmpty)
{
    // combnk(1:4,5): K=5 > N=4 -> empty 0x5 (MATLAB), not an error.
    EXPECT_EQ(static_cast<int>(evalScalar("size(combnk(1:4,5),1)")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("size(combnk(1:4,5),2)")), 5);
}

TEST_F(CombnkTest, KZeroIsOneEmptyCombination)
{
    // combnk(1:4,0) -> 1x0 (one empty combination).
    EXPECT_EQ(static_cast<int>(evalScalar("size(combnk(1:4,0),1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(combnk(1:4,0),2)")), 0);
}

TEST_F(CombnkTest, VectorFormUnchanged)
{
    // combnk(1:4,2) -> C(4,2)=6 combinations, 2 columns (unchanged).
    EXPECT_EQ(static_cast<int>(evalScalar("size(combnk(1:4,2),1)")), 6);
    EXPECT_EQ(static_cast<int>(evalScalar("size(combnk(1:4,2),2)")), 2);
    // First combination is [1 2] in lex order.
    EXPECT_NEAR(evalScalar("max(max(combnk(1:4,2)))"), 4.0, 1e-12);
}
