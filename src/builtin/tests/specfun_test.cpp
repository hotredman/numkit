#include <gtest/gtest.h>
#include <numkit/bundle/standard_library.hpp>
#include <numkit/core/engine.hpp>

using namespace numkit;

class SpecfunTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = makeStandardEngine();
    }

    std::unique_ptr<Engine> engine;
};

TEST_F(SpecfunTest, CombinatoricsAndPrimes) {
    Value f = engine->eval("factorial(5);");
    EXPECT_DOUBLE_EQ(f.toScalar(), 120.0);

    Value p = engine->eval("isprime(7);");
    EXPECT_TRUE(p.toBool());

    Value c = engine->eval("nchoosek(5, 2);");
    EXPECT_DOUBLE_EQ(c.toScalar(), 10.0);
}

TEST_F(SpecfunTest, GammaAndErf) {
    Value g = engine->eval("gamma(5);");
    EXPECT_DOUBLE_EQ(g.toScalar(), 24.0);

    Value e = engine->eval("erf(0);");
    EXPECT_DOUBLE_EQ(e.toScalar(), 0.0);
}
