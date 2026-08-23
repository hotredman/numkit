#include <gtest/gtest.h>
#include <numkit/bundle/standard_library.hpp>
#include <numkit/core/engine.hpp>

using namespace numkit;

class TimefunTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = makeStandardEngine();
    }

    std::unique_ptr<Engine> engine;
};

TEST_F(TimefunTest, TicTocMeasurement) {
    engine->eval("tic();");
    Value elapsed = engine->eval("t = toc();");
    EXPECT_GE(elapsed.toScalar(), 0.0);
}

TEST_F(TimefunTest, NowAndCpuTime) {
    Value nw = engine->eval("now();");
    EXPECT_GT(nw.toScalar(), 700000.0);

    Value cp = engine->eval("cputime();");
    EXPECT_GE(cp.toScalar(), 0.0);
}
