#include <gtest/gtest.h>
#include <numkit/builtin/ops.hpp>
#include <numkit/bundle/standard_library.hpp>
#include <numkit/core/engine.hpp>

using namespace numkit;

class OpsTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = makeStandardEngine();
    }

    std::unique_ptr<Engine> engine;
};

TEST_F(OpsTest, BinaryArithmetic) {
    Value r1 = engine->eval("3 + 4;");
    EXPECT_DOUBLE_EQ(r1.toScalar(), 7.0);

    Value r2 = engine->eval("10 - 4;");
    EXPECT_DOUBLE_EQ(r2.toScalar(), 6.0);

    Value r3 = engine->eval("3 * 5;");
    EXPECT_DOUBLE_EQ(r3.toScalar(), 15.0);

    Value r4 = engine->eval("15 / 3;");
    EXPECT_DOUBLE_EQ(r4.toScalar(), 5.0);

    Value r5 = engine->eval("2 ^ 3;");
    EXPECT_DOUBLE_EQ(r5.toScalar(), 8.0);
}

TEST_F(OpsTest, DirectCppArithmeticAPI) {
    Value a = Value::scalar(10.0);
    Value b = Value::scalar(4.0);

    EXPECT_DOUBLE_EQ(builtin::plus(a, b).toScalar(), 14.0);
    EXPECT_DOUBLE_EQ(builtin::minus(a, b).toScalar(), 6.0);
    EXPECT_DOUBLE_EQ(builtin::times(a, b).toScalar(), 40.0);
    EXPECT_DOUBLE_EQ(builtin::rdivide(a, b).toScalar(), 2.5);
    EXPECT_DOUBLE_EQ(builtin::ldivide(b, a).toScalar(), 2.5); // ldivide(4, 10) = 10 / 4
    EXPECT_DOUBLE_EQ(builtin::mrdivide(a, b).toScalar(), 2.5);
    EXPECT_DOUBLE_EQ(builtin::mldivide(b, a).toScalar(), 2.5);
    EXPECT_DOUBLE_EQ(builtin::power(a, Value::scalar(2.0)).toScalar(), 100.0);
    EXPECT_DOUBLE_EQ(builtin::mpower(a, Value::scalar(2.0)).toScalar(), 100.0);
    EXPECT_DOUBLE_EQ(builtin::uplus(a).toScalar(), 10.0);
    EXPECT_DOUBLE_EQ(builtin::uminus(a).toScalar(), -10.0);

    // Matrix multiplication
    Value matA = engine->eval("[1 2; 3 4];");
    Value matB = engine->eval("[2 0; 1 2];");
    Value matC = builtin::mtimes(matA, matB);
    EXPECT_EQ(matC.dims().rows(), 2u);
    EXPECT_EQ(matC.dims().cols(), 2u);
    EXPECT_DOUBLE_EQ(matC.elemAsDouble(0), 4.0);  // 1*2 + 2*1 = 4
    EXPECT_DOUBLE_EQ(matC.elemAsDouble(1), 10.0); // 3*2 + 4*1 = 10
    EXPECT_DOUBLE_EQ(matC.elemAsDouble(2), 4.0);  // 1*0 + 2*2 = 4
    EXPECT_DOUBLE_EQ(matC.elemAsDouble(3), 8.0);  // 3*0 + 4*2 = 8

    // pagemtimes
    Value pageC = builtin::pagemtimes(matA, matB);
    EXPECT_EQ(pageC.dims().rows(), 2u);
    EXPECT_EQ(pageC.dims().cols(), 2u);
    EXPECT_DOUBLE_EQ(pageC.elemAsDouble(0), 4.0);
}

TEST_F(OpsTest, DirectCppRelationalAPI) {
    Value s3 = Value::scalar(3.0);
    Value s5 = Value::scalar(5.0);

    EXPECT_TRUE(builtin::eq(s5, s5).toBool());
    EXPECT_FALSE(builtin::eq(s5, s3).toBool());
    EXPECT_TRUE(builtin::ne(s5, s3).toBool());
    EXPECT_FALSE(builtin::ne(s5, s5).toBool());

    EXPECT_TRUE(builtin::lt(s3, s5).toBool());
    EXPECT_FALSE(builtin::lt(s5, s3).toBool());
    EXPECT_FALSE(builtin::lt(s5, s5).toBool());

    EXPECT_TRUE(builtin::le(s3, s5).toBool());
    EXPECT_TRUE(builtin::le(s5, s5).toBool());
    EXPECT_FALSE(builtin::le(s5, s3).toBool());

    EXPECT_TRUE(builtin::gt(s5, s3).toBool());
    EXPECT_FALSE(builtin::gt(s3, s5).toBool());
    EXPECT_FALSE(builtin::gt(s5, s5).toBool());

    EXPECT_TRUE(builtin::ge(s5, s3).toBool());
    EXPECT_TRUE(builtin::ge(s5, s5).toBool());
    EXPECT_FALSE(builtin::ge(s3, s5).toBool());

    // Matrix broadcasting comparison
    Value mat = engine->eval("[1 5; 3 7];");
    Value gt4 = builtin::gt(mat, Value::scalar(4.0));
    EXPECT_EQ(gt4.dims().rows(), 2u);
    EXPECT_EQ(gt4.dims().cols(), 2u);
    EXPECT_FALSE(gt4.logicalData()[0]); // 1 > 4 -> false
    EXPECT_FALSE(gt4.logicalData()[1]); // 3 > 4 -> false
    EXPECT_TRUE(gt4.logicalData()[2]);  // 5 > 4 -> true
    EXPECT_TRUE(gt4.logicalData()[3]);  // 7 > 4 -> true
}

TEST_F(OpsTest, DirectCppLogicalAndTransposeAPI) {
    Value t = Value::logicalScalar(true);
    Value f = Value::logicalScalar(false);

    EXPECT_FALSE(builtin::logical_and(t, f).toBool());
    EXPECT_FALSE(builtin::and_op(t, f).toBool());
    EXPECT_TRUE(builtin::logical_and(t, t).toBool());

    EXPECT_TRUE(builtin::logical_or(t, f).toBool());
    EXPECT_TRUE(builtin::or_op(t, f).toBool());
    EXPECT_FALSE(builtin::logical_or(f, f).toBool());

    EXPECT_FALSE(builtin::logical_not(t).toBool());
    EXPECT_FALSE(builtin::not_op(t).toBool());
    EXPECT_TRUE(builtin::logical_not(f).toBool());

    EXPECT_TRUE(builtin::logical_xor(t, f).toBool());
    EXPECT_TRUE(builtin::xor_op(t, f).toBool());
    EXPECT_FALSE(builtin::logical_xor(t, t).toBool());
    EXPECT_FALSE(builtin::logical_xor(f, f).toBool());

    // any / all reductions
    Value vZero = engine->eval("[0 0 0];");
    Value vMixed = engine->eval("[0 1 0];");
    Value vOnes = engine->eval("[1 1 1];");

    EXPECT_FALSE(builtin::any(vZero).toBool());
    EXPECT_TRUE(builtin::any(vMixed).toBool());
    EXPECT_TRUE(builtin::all(vOnes).toBool());
    EXPECT_FALSE(builtin::all(vMixed).toBool());

    // Transposition
    Value mat = engine->eval("[1 2 3; 4 5 6];"); // 2x3
    Value matT = builtin::transpose(mat);        // 3x2
    EXPECT_EQ(matT.dims().rows(), 3u);
    EXPECT_EQ(matT.dims().cols(), 2u);
    EXPECT_DOUBLE_EQ(matT.elemAsDouble(0), 1.0);
    EXPECT_DOUBLE_EQ(matT.elemAsDouble(1), 2.0);
    EXPECT_DOUBLE_EQ(matT.elemAsDouble(2), 3.0);
    EXPECT_DOUBLE_EQ(matT.elemAsDouble(3), 4.0);

    Value matCT = builtin::ctranspose(mat);
    EXPECT_EQ(matCT.dims().rows(), 3u);
    EXPECT_EQ(matCT.dims().cols(), 2u);
}

TEST_F(OpsTest, RelationalAndLogical) {
    Value r1 = engine->eval("5 > 3;");
    EXPECT_TRUE(r1.toBool());

    Value r2 = engine->eval("2 == 3;");
    EXPECT_FALSE(r2.toBool());

    Value r3 = engine->eval("1 & 0;");
    EXPECT_FALSE(r3.toBool());

    Value r4 = engine->eval("1 | 0;");
    EXPECT_TRUE(r4.toBool());

    Value r5 = engine->eval("~1;");
    EXPECT_FALSE(r5.toBool());
}

TEST_F(OpsTest, FunctionalAliases) {
    Value r1 = engine->eval("plus(10, 20);");
    EXPECT_DOUBLE_EQ(r1.toScalar(), 30.0);

    Value r2 = engine->eval("times(3, 7);");
    EXPECT_DOUBLE_EQ(r2.toScalar(), 21.0);

    Value r3 = engine->eval("xor([1 0], [0 0]);");
    EXPECT_GT(r3.numel(), 0u);
}
