#include <gtest/gtest.h>
#include <numkit/bundle/standard_library.hpp>
#include <numkit/core/engine.hpp>

using namespace numkit;

class DatatypesTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = makeStandardEngine();
    }

    std::unique_ptr<Engine> engine;
};

TEST_F(DatatypesTest, NumericConversions) {
    Value d = engine->eval("double(int32(42));");
    EXPECT_DOUBLE_EQ(d.toScalar(), 42.0);

    Value i = engine->eval("int32(3.14);");
    EXPECT_EQ(i.type(), ValueType::INT32);

    Value l = engine->eval("logical([1 0 1]);");
    EXPECT_TRUE(l.isLogical());
}

TEST_F(DatatypesTest, StructuresAndCells) {
    Value s = engine->eval("s = struct('a', 10, 'b', 'test'); s.a;");
    EXPECT_DOUBLE_EQ(s.toScalar(), 10.0);

    Value c = engine->eval("c = {1, 'hello', [1 2 3]}; c{1};");
    EXPECT_DOUBLE_EQ(c.toScalar(), 1.0);
}
