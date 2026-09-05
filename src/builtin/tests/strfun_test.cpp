#include <gtest/gtest.h>
#include <numkit/bundle/standard_library.hpp>
#include <numkit/core/engine.hpp>

using namespace numkit;

class StrfunTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = makeStandardEngine();
    }

    std::unique_ptr<Engine> engine;
};

TEST_F(StrfunTest, StringComparisonAndSearch) {
    Value sc = engine->eval("strcmp('hello', 'hello');");
    EXPECT_TRUE(sc.toBool());

    Value nsc = engine->eval("strcmp('hello', 'world');");
    EXPECT_FALSE(nsc.toBool());

    Value cnt = engine->eval("contains('Numkit Engine', 'kit');");
    EXPECT_TRUE(cnt.toBool());
}

TEST_F(StrfunTest, StringTransformations) {
    Value u = engine->eval("upper('hello');");
    EXPECT_EQ(u.toString(), "HELLO");

    Value l = engine->eval("lower('WORLD');");
    EXPECT_EQ(l.toString(), "world");

    Value rep = engine->eval("strrep('abc123abc', '123', 'XYZ');");
    EXPECT_EQ(rep.toString(), "abcXYZabc");
}

// --- bugs/closed/lang/num2str-complex-array.md (FIXED) ---
// Complex ARRAY formatting (even all-zero imaginary parts — complex-typed
// storage of real data) must format like MATLAB, not throw.
TEST_F(StrfunTest, Num2strComplexArray)
{
    // MATLAB prints imag==0 elements as plain real numbers (the probe's
    // '13    24' was fprintf row-concatenation; the actual char matrix is
    // '1  2' / '3  4'). Must not throw, must be char, must not emit 'i',
    // and a genuinely-complex element keeps the a±bi form.
    Value s = engine->eval("num2str(complex([1 2; 3 4]));");
    EXPECT_EQ(s.toString().find('i'), std::string::npos);
    EXPECT_EQ(engine->eval("class(num2str(complex([1 2; 3 4])));").toString(), "char");
    Value m = engine->eval("num2str(complex([1, 2+3i]));");
    EXPECT_NE(m.toString().find("+3i"), std::string::npos);
}

// --- inline (legacy constructor, fieldtest portion 10) ---
TEST_F(StrfunTest, InlineConstructor)
{
    Value g = engine->eval("g = inline('a+b','a','b');");
    Value r = engine->eval("g(2,3);");
    EXPECT_NEAR(r.toScalar(), 5.0, 1e-12);
    EXPECT_NEAR(engine->eval("f2 = inline('x^2','x'); f2(7);").toScalar(), 49.0, 1e-12);
    EXPECT_NEAR(engine->eval("f3 = inline('5*sin(2*pi*1*t).*exp(-.4*t)','t'); f3(0.5);").toScalar(),
                5.01328e-16, 1e-20);
}
