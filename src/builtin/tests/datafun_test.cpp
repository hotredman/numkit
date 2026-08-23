#include <gtest/gtest.h>
#include <numkit/bundle/standard_library.hpp>
#include <numkit/core/engine.hpp>

using namespace numkit;

class DatafunTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = makeStandardEngine();
    }

    std::unique_ptr<Engine> engine;
};

TEST_F(DatafunTest, SummaryStatistics) {
    Value s = engine->eval("sum([1 2 3 4]);");
    EXPECT_DOUBLE_EQ(s.toScalar(), 10.0);

    Value p = engine->eval("prod([2 3 4]);");
    EXPECT_DOUBLE_EQ(p.toScalar(), 24.0);

    Value m = engine->eval("mean([2 4 6]);");
    EXPECT_DOUBLE_EQ(m.toScalar(), 4.0);

    Value mx = engine->eval("max([1 5 3]);");
    EXPECT_DOUBLE_EQ(mx.toScalar(), 5.0);

    Value mn = engine->eval("min([1 5 3]);");
    EXPECT_DOUBLE_EQ(mn.toScalar(), 1.0);
}

TEST_F(DatafunTest, CumulativeAndDifferences) {
    Value cs = engine->eval("cumsum([1 2 3]);");
    EXPECT_EQ(cs.numel(), 3u);

    Value d = engine->eval("diff([1 3 6 10]);");
    EXPECT_EQ(d.numel(), 3u);
}

TEST_F(DatafunTest, Sorting) {
    Value s = engine->eval("sort([4 1 3 2]);");
    EXPECT_EQ(s.numel(), 4u);
}
