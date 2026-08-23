#include <gtest/gtest.h>
#include <numkit/builtin/matfun.hpp>
#include <numkit/bundle/standard_library.hpp>
#include <numkit/core/engine.hpp>

using namespace numkit;

class MatfunTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = makeStandardEngine();
    }

    std::unique_ptr<Engine> engine;
};

TEST_F(MatfunTest, MatrixInverseAndDeterminant) {
    Value d = engine->eval("det([1 2; 3 4]);");
    EXPECT_NEAR(d.toScalar(), -2.0, 1e-12);

    Value invA = engine->eval("inv([4 7; 2 6]);");
    EXPECT_EQ(invA.dims().rows(), 2u);
    EXPECT_EQ(invA.dims().cols(), 2u);
}

TEST_F(MatfunTest, MatrixDecompositions) {
    Value s = engine->eval("s = svd([1 2; 3 4]);");
    EXPECT_EQ(s.numel(), 2u);

    Value e = engine->eval("e = eig([2 0; 0 3]);");
    EXPECT_EQ(e.numel(), 2u);

    Value n = engine->eval("norm([3 4]);");
    EXPECT_DOUBLE_EQ(n.toScalar(), 5.0);

    Value tr = engine->eval("trace([1 2; 3 4]);");
    EXPECT_DOUBLE_EQ(tr.toScalar(), 5.0);
}

TEST_F(MatfunTest, DirectCppMatfunAPI) {
    Value a = engine->eval("int32(10);");
    Value b = engine->eval("int32(3);");

    Value qFix = builtin::idivide(a, b, "fix");
    EXPECT_DOUBLE_EQ(qFix.toScalar(), 3.0);

    Value qFloor = builtin::idivide(a, b, "floor");
    EXPECT_DOUBLE_EQ(qFloor.toScalar(), 3.0);

    Value qCeil = builtin::idivide(a, b, "ceil");
    EXPECT_DOUBLE_EQ(qCeil.toScalar(), 4.0);

    Value qRound = builtin::idivide(a, b, "round");
    EXPECT_DOUBLE_EQ(qRound.toScalar(), 3.0);
}
