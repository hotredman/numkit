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

// --- bugs/opened/lang/num2str-complex-array.md ---
// Complex ARRAY formatting (even all-zero imaginary parts — complex-typed
// storage of real data) must format like MATLAB, not throw.
TEST_F(StrfunTest, DISABLED_Num2strComplexArray)
{
    // MATLAB: '13    24' — imag==0 prints as real. Must not throw and
    // must not emit an 'i'.
    Value s = engine->eval("num2str(complex([1 2; 3 4]));");
    EXPECT_EQ(s.toString().find('i'), std::string::npos);
    EXPECT_NE(s.toString().find("13"), std::string::npos);
}
