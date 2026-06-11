// toolboxes/stats/tests/crossval_test.cpp
//
// Regression guard for crossval (k-fold cross-validation).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class CrossvalTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(CrossvalTest, DefaultTenFold)
{
    eval("X = (1:20)'; Y = 2*X + 1; "
         "predfun = @(Xtr, Ytr, Xte, Yte) mean((Xte * mldivide(Xtr, Ytr) - Yte).^2); "
         "v = crossval(predfun, X, Y);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(v, 1)")), 10);
    EXPECT_EQ(static_cast<int>(evalScalar("size(v, 2)")), 1);
}

TEST_F(CrossvalTest, KFoldOption)
{
    eval("X = (1:20)'; Y = 2*X + 1; "
         "predfun = @(Xtr, Ytr, Xte, Yte) mean((Xte * mldivide(Xtr, Ytr) - Yte).^2); "
         "v5 = crossval(predfun, X, Y, 'kfold', 5);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(v5, 1)")), 5);
}

TEST_F(CrossvalTest, PerfectModelGivesLowMSE)
{
    // y = x identity, predfun returns slope*xte: should be ~0 MSE.
    eval("X = (1:20)'; Y = X; "
         "predfun = @(Xtr, Ytr, Xte, Yte) mean((Xte * mldivide(Xtr, Ytr) - Yte).^2); "
         "v = crossval(predfun, X, Y);");
    EXPECT_LT(evalScalar("mean(v)"), 1e-10);
}

TEST_F(CrossvalTest, RequiresFuncHandle)
{
    EXPECT_THROW(eval("crossval(5, [1;2;3], [1;2;3]);"), std::exception);
}

TEST_F(CrossvalTest, DimMismatchRejected)
{
    eval("predfun = @(a, b, c, d) 1.0;");
    EXPECT_THROW(eval("crossval(predfun, [1;2;3], [1;2]);"), std::exception);
}

TEST_F(CrossvalTest, UnknownOptionRejected)
{
    eval("predfun = @(a, b, c, d) 1.0;");
    EXPECT_THROW(eval("crossval(predfun, [1;2;3;4], [1;2;3;4], 'unknown', 1);"), std::exception);
}
