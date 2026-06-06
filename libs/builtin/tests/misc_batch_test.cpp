// libs/builtin/tests/misc_batch_test.cpp
// : predicates + coord conversion + airy.
//   allfinite / allunique / anynan
//   airy
//   cart2pol / cart2sph
// All  — bit-identical MATLAB R2025b
// on probed inputs (parity tol=1e-12).

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class MiscBatchTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(MiscBatchTest, AllFinite)
{
    EXPECT_DOUBLE_EQ(evalScalar("allfinite([1, 2, 3])"),    1.0);
    EXPECT_DOUBLE_EQ(evalScalar("allfinite([1, NaN, 3])"),  0.0);
    EXPECT_DOUBLE_EQ(evalScalar("allfinite([1, Inf, 3])"),  0.0);
    EXPECT_DOUBLE_EQ(evalScalar("allfinite([1, -Inf, 3])"), 0.0);
}

TEST_F(MiscBatchTest, AllUnique)
{
    EXPECT_DOUBLE_EQ(evalScalar("allunique([1, 2, 3])"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("allunique([1, 2, 1])"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("allunique([5, 5])"),    0.0);
    EXPECT_DOUBLE_EQ(evalScalar("allunique([1])"),       1.0);
}

TEST_F(MiscBatchTest, AnyNan)
{
    EXPECT_DOUBLE_EQ(evalScalar("anynan([1, 2, 3])"),   0.0);
    EXPECT_DOUBLE_EQ(evalScalar("anynan([1, NaN, 3])"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("anynan(NaN)"),         1.0);
    EXPECT_DOUBLE_EQ(evalScalar("anynan([1, Inf, 3])"), 0.0);
}

TEST_F(MiscBatchTest, Airy)
{
    // Ai(0) = 1/(3^(2/3) * gamma(2/3)) = 0.355028053887817
    EXPECT_NEAR(evalScalar("airy(0)"),  0.355028053887817, 1e-9);
    EXPECT_NEAR(evalScalar("airy(1)"),  0.135292416312881, 1e-9);
    EXPECT_NEAR(evalScalar("airy(-1)"), 0.535560883292352, 1e-9);
}

TEST_F(MiscBatchTest, Cart2Pol)
{
    // [th, rh] = cart2pol(1, 0) → th=0, rh=1
    eval("[th, rh] = cart2pol(1, 0);");
    EXPECT_NEAR(evalScalar("th"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("rh"), 1.0, 1e-12);
    // [th, rh] = cart2pol(0, 1) → th=pi/2, rh=1
    eval("[th, rh] = cart2pol(0, 1);");
    EXPECT_NEAR(evalScalar("th"), 1.570796326794897, 1e-12);
    EXPECT_NEAR(evalScalar("rh"), 1.0, 1e-12);
}

TEST_F(MiscBatchTest, Cart2Sph)
{
    // [az, el, rh] = cart2sph(1, 0, 0) → az=0, el=0, rh=1
    eval("[az, el, rh] = cart2sph(1, 0, 0);");
    EXPECT_NEAR(evalScalar("az"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("el"), 0.0, 1e-12);
    EXPECT_NEAR(evalScalar("rh"), 1.0, 1e-12);
    // [az, el, rh] = cart2sph(0, 0, 1) → az=0, el=pi/2, rh=1
    eval("[az, el, rh] = cart2sph(0, 0, 1);");
    EXPECT_NEAR(evalScalar("el"), 1.570796326794897, 1e-12);
}
