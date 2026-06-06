// libs/signal/tests/dct_types_test.cpp
//
// Regression guard for bugs/signal/dct-types.md (fixed): dct / idct now
// implement the orthonormal DCT variants Type 1, 3 and 4 (Type 2 is the
// default). Type 1 and Type 4 are self-inverse; Type 2 and Type 3 are
// each other's inverse. Expected values from MATLAB R2025b.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class DctTypesTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(DctTypesTest, ForwardType1)
{
    eval("y = dct([1 2 3 4], 4, 'Type', 1);");
    EXPECT_NEAR(evalScalar("y(1)"),  4.927993, 1e-6);
    EXPECT_NEAR(evalScalar("y(2)"), -2.140299, 1e-6);
    EXPECT_NEAR(evalScalar("y(3)"),  0.845510, 1e-6);
    EXPECT_NEAR(evalScalar("y(4)"), -0.647395, 1e-6);
}

TEST_F(DctTypesTest, ForwardType3)
{
    eval("y = dct([1 2 3 4], 4, 'Type', 3);");
    EXPECT_NEAR(evalScalar("y(1)"),  4.388955, 1e-6);
    EXPECT_NEAR(evalScalar("y(2)"), -3.071930, 1e-6);
    EXPECT_NEAR(evalScalar("y(3)"),  1.071930, 1e-6);
    EXPECT_NEAR(evalScalar("y(4)"), -0.388955, 1e-6);
}

TEST_F(DctTypesTest, ForwardType4)
{
    eval("y = dct([1 2 3 4], 4, 'Type', 4);");
    EXPECT_NEAR(evalScalar("y(1)"),  3.599737, 1e-6);
    EXPECT_NEAR(evalScalar("y(2)"), -3.339911, 1e-6);
    EXPECT_NEAR(evalScalar("y(3)"),  1.771408, 1e-6);
    EXPECT_NEAR(evalScalar("y(4)"), -1.658012, 1e-6);
}

TEST_F(DctTypesTest, InverseType3IsDctType2)
{
    // idct Type 3 == dct Type 2 (forward DCT-II). MATLAB: [5 -2.230442 0 ...].
    eval("y = idct([1 2 3 4], 4, 'Type', 3);");
    EXPECT_NEAR(evalScalar("y(1)"),  5.000000, 1e-6);
    EXPECT_NEAR(evalScalar("y(2)"), -2.230442, 1e-6);
    EXPECT_NEAR(evalScalar("y(3)"),  0.000000, 1e-6);
    EXPECT_NEAR(evalScalar("y(4)"), -0.158513, 1e-6);
}

TEST_F(DctTypesTest, SelfInverseTypes1And4)
{
    // Type 1 and Type 4 are self-inverse: idct(...,'Type',t)==dct(...,'Type',t).
    eval("y1f = dct([1 2 3 4], 4, 'Type', 1); y1i = idct([1 2 3 4], 4, 'Type', 1);");
    EXPECT_NEAR(evalScalar("max(abs(y1f - y1i))"), 0.0, 1e-12);
    eval("y4f = dct([1 2 3 4], 4, 'Type', 4); y4i = idct([1 2 3 4], 4, 'Type', 4);");
    EXPECT_NEAR(evalScalar("max(abs(y4f - y4i))"), 0.0, 1e-12);
}

TEST_F(DctTypesTest, RoundTripAllTypes)
{
    // idct(dct(x,Type t),Type t) recovers x for every type.
    for (int t = 1; t <= 4; ++t) {
        const std::string s = std::to_string(t);
        eval("xr = idct(dct([1 2 3 4], 4, 'Type', " + s +
             "), 4, 'Type', " + s + ");");
        EXPECT_NEAR(evalScalar("max(abs(xr - [1 2 3 4]))"), 0.0, 1e-10)
            << "round-trip failed for Type " << t;
    }
}

TEST_F(DctTypesTest, NonPowerOfTwoType4)
{
    // Direct path (N=5 not a power of two). MATLAB reference.
    eval("y = dct([1 2 3 4 5], 5, 'Type', 4);");
    EXPECT_NEAR(evalScalar("y(1)"),  4.736558, 1e-6);
    EXPECT_NEAR(evalScalar("y(5)"),  1.735578, 1e-6);
}

TEST_F(DctTypesTest, MatrixColumnwiseType1)
{
    // dct applies column-wise to a matrix. MATLAB col 1: [5.121320 ...].
    eval("M = dct([1 2; 3 4; 5 6], 3, 'Type', 1);");
    EXPECT_NEAR(evalScalar("M(1,1)"),  5.121320, 1e-6);
    EXPECT_NEAR(evalScalar("M(2,1)"), -2.828427, 1e-6);
    EXPECT_NEAR(evalScalar("M(3,1)"),  0.878680, 1e-6);
}

TEST_F(DctTypesTest, InvalidTypeThrows)
{
    EXPECT_THROW(eval("dct([1 2 3 4], 4, 'Type', 5);"), std::exception);
}
