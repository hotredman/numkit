// libs/builtin/tests/issparse_test.cpp
//
// Regression guard for issparse stub. Numkit has no sparse-matrix
// storage class, so issparse always returns false. Matches MATLAB on
// dense inputs (which is all numkit can produce).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class IssparseTest : public ::testing::Test
{
public:
    StandardEngine engine;
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(IssparseTest, AlwaysFalse_DenseDouble)
{
    EXPECT_DOUBLE_EQ(evalScalar("double(issparse([1 2; 3 4]))"), 0.0);
}

TEST_F(IssparseTest, AlwaysFalse_Zeros)
{
    EXPECT_DOUBLE_EQ(evalScalar("double(issparse(zeros(3,3)))"), 0.0);
}

TEST_F(IssparseTest, AlwaysFalse_Eye)
{
    EXPECT_DOUBLE_EQ(evalScalar("double(issparse(eye(3)))"), 0.0);
}

TEST_F(IssparseTest, AlwaysFalse_Scalar)
{
    EXPECT_DOUBLE_EQ(evalScalar("double(issparse(0))"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(issparse(3.14))"), 0.0);
}

TEST_F(IssparseTest, AlwaysFalse_Empty)
{
    EXPECT_DOUBLE_EQ(evalScalar("double(issparse([]))"), 0.0);
}

TEST_F(IssparseTest, AlwaysFalse_Logical)
{
    EXPECT_DOUBLE_EQ(evalScalar("double(issparse(true))"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(issparse([true false true]))"), 0.0);
}

TEST_F(IssparseTest, AlwaysFalse_Char)
{
    EXPECT_DOUBLE_EQ(evalScalar("double(issparse('abc'))"), 0.0);
}

TEST_F(IssparseTest, AlwaysFalse_Cell)
{
    EXPECT_DOUBLE_EQ(evalScalar("double(issparse({1, 2, 'x'}))"), 0.0);
}

TEST_F(IssparseTest, ReturnsLogicalScalar)
{
    Value y = eval("issparse([1 2 3])");
    EXPECT_TRUE(y.isLogical());
    EXPECT_TRUE(y.isScalar());
}
