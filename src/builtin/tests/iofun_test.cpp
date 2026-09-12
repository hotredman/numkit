#include <gtest/gtest.h>
#include <nk/bundle/standard_library.hpp>
#include <nk/core/engine.hpp>

using namespace nk;

class IofunTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = makeStandardEngine();
            }

    std::unique_ptr<Engine> engine;
};

TEST_F(IofunTest, PrintfAndScanfFormat) {
    Value s = engine->eval("sprintf('Value: %d, %s', 42, 'test');");
    EXPECT_EQ(s.toString(), "Value: 42, test");

    Value sc = engine->eval("[v, count] = sscanf('123 456', '%d %d');");
    EXPECT_GT(sc.numel(), 0u);
}
