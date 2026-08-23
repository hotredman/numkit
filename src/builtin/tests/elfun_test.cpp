#include <gtest/gtest.h>
#include <numkit/bundle/standard_library.hpp>
#include <numkit/core/engine.hpp>

using namespace numkit;

class ElfunTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = makeStandardEngine();
    }

    std::unique_ptr<Engine> engine;
};

TEST_F(ElfunTest, TrigonometricFunctions) {
    Value s = engine->eval("sin(pi / 2);");
    EXPECT_NEAR(s.toScalar(), 1.0, 1e-12);

    Value c = engine->eval("cos(0);");
    EXPECT_NEAR(c.toScalar(), 1.0, 1e-12);

    Value t = engine->eval("tan(0);");
    EXPECT_NEAR(t.toScalar(), 0.0, 1e-12);

    Value a = engine->eval("asin(1);");
    EXPECT_NEAR(a.toScalar(), 3.141592653589793 / 2.0, 1e-12);
}

TEST_F(ElfunTest, ExponentialAndLogarithmic) {
    Value e = engine->eval("exp(1);");
    EXPECT_NEAR(e.toScalar(), 2.718281828459045, 1e-12);

    Value l = engine->eval("log(exp(3));");
    EXPECT_NEAR(l.toScalar(), 3.0, 1e-12);

    Value sq = engine->eval("sqrt(144);");
    EXPECT_DOUBLE_EQ(sq.toScalar(), 12.0);
}

TEST_F(ElfunTest, ComplexNumbers) {
    Value c = engine->eval("z = complex(3, 4); abs(z);");
    EXPECT_DOUBLE_EQ(c.toScalar(), 5.0);

    Value r = engine->eval("real(3 + 4i);");
    EXPECT_DOUBLE_EQ(r.toScalar(), 3.0);

    Value im = engine->eval("imag(3 + 4i);");
    EXPECT_DOUBLE_EQ(im.toScalar(), 4.0);
}

TEST_F(ElfunTest, RoundingAndRemainder) {
    Value fl = engine->eval("floor(3.7);");
    EXPECT_DOUBLE_EQ(fl.toScalar(), 3.0);

    Value ce = engine->eval("ceil(3.2);");
    EXPECT_DOUBLE_EQ(ce.toScalar(), 4.0);

    Value m = engine->eval("mod(10, 3);");
    EXPECT_DOUBLE_EQ(m.toScalar(), 1.0);
}
