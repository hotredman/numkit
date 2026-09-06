#include <gtest/gtest.h>
#include <numkit/builtin/builtin.hpp>
#include <numkit/bundle/builtin_library.hpp>
#include <numkit/core/engine.hpp>

using namespace numkit;

// Suite name must NOT collide with the parameterized BuiltinTest in
// bundle/tests/builtins_test.cpp: two different fixture classes under
// one gtest suite name is documented UB (mixed TEST_F/TEST_P) — it
// crashed every Debug-config run with an access violation while
// Release survived (bugs/closed/core/debug-build-setup-crash).
class BuiltinLayerTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = std::make_unique<Engine>();
        BuiltinLibrary::install(*engine);
    }

    std::unique_ptr<Engine> engine;
};

TEST_F(BuiltinLayerTest, FullLibraryInstalled) {
    // Math
    EXPECT_DOUBLE_EQ(engine->eval("sin(0);").toScalar(), 0.0);
    EXPECT_DOUBLE_EQ(engine->eval("sqrt(100);").toScalar(), 10.0);

    // Matrix
    EXPECT_EQ(engine->eval("zeros(2, 2);").dims().rows(), 2u);
    EXPECT_EQ(engine->eval("eye(3);").dims().cols(), 3u);

    // Ops
    EXPECT_DOUBLE_EQ(engine->eval("10 + 20;").toScalar(), 30.0);

    // String
    EXPECT_TRUE(engine->eval("strcmp('a', 'a');").toBool());

    // Datafun
    EXPECT_DOUBLE_EQ(engine->eval("sum([1 2 3]);").toScalar(), 6.0);
    EXPECT_DOUBLE_EQ(engine->eval("diff([10 25]);").toScalar(), 15.0);
}
