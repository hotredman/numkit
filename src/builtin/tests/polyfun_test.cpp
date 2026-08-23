#include <gtest/gtest.h>
#include <numkit/bundle/standard_library.hpp>
#include <numkit/core/engine.hpp>

using namespace numkit;

class PolyfunTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = makeStandardEngine();
    }

    std::unique_ptr<Engine> engine;
};

TEST_F(PolyfunTest, PolynomialEvaluationAndRoots) {
    Value r = engine->eval("roots([1 -5 6]);");
    EXPECT_EQ(r.numel(), 2u);

    Value p = engine->eval("polyval([1 0 1], 2);");
    EXPECT_DOUBLE_EQ(p.toScalar(), 5.0);
}

TEST_F(PolyfunTest, InterpolationAndIntegration) {
    Value v = engine->eval("interp1([0 1 2], [0 10 20], 1.5);");
    EXPECT_DOUBLE_EQ(v.toScalar(), 15.0);

    Value t = engine->eval("trapz([1 2 3], [1 4 9]);");
    EXPECT_NEAR(t.toScalar(), 9.0, 1e-12);
}
