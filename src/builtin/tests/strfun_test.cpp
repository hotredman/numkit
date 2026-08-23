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
