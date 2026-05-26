// libs/stats/tests/lhsdesign_lhsnorm_test.cpp
//
// Regression guard for lhsdesign + lhsnorm.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class LhsTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override { engine.eval("import compat.*; rng(0);"); }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── lhsdesign ───────────────────────────────────────────────────────

TEST_F(LhsTest, LhsdesignShape)
{
    eval("X = lhsdesign(7, 4);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(X, 1)")), 7);
    EXPECT_EQ(static_cast<int>(evalScalar("size(X, 2)")), 4);
}

TEST_F(LhsTest, LhsdesignAllInUnitInterval)
{
    eval("X = lhsdesign(20, 5); ok = all(X(:) > 0 & X(:) < 1);");
    EXPECT_EQ(static_cast<int>(evalScalar("ok")), 1);
}

TEST_F(LhsTest, LhsdesignBinPartition)
{
    // Each column must partition [0, 1] into n bins; one sample per bin.
    eval(R"(
        n = 10;
        X = lhsdesign(n, 3);
        bins = ceil(X * n);
        ok = 1;
        for j = 1:3
            s = sort(bins(:, j))';
            if any(s ~= 1:n)
                ok = 0;
            end
        end
    )");
    EXPECT_EQ(static_cast<int>(evalScalar("ok")), 1);
}

TEST_F(LhsTest, LhsdesignMeanCloseToHalf)
{
    eval("X = lhsdesign(200, 2); m = mean(X);");
    EXPECT_NEAR(evalScalar("m(1)"), 0.5, 0.05);
    EXPECT_NEAR(evalScalar("m(2)"), 0.5, 0.05);
}

// ── lhsnorm ─────────────────────────────────────────────────────────

TEST_F(LhsTest, LhsnormShape)
{
    eval("Y = lhsnorm([1 2 3], eye(3), 50);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(Y, 1)")), 50);
    EXPECT_EQ(static_cast<int>(evalScalar("size(Y, 2)")), 3);
}

TEST_F(LhsTest, LhsnormMeanRecovered)
{
    eval("Y = lhsnorm([10, -5], eye(2), 500); m = mean(Y);");
    EXPECT_NEAR(evalScalar("m(1)"), 10.0, 1.0);
    EXPECT_NEAR(evalScalar("m(2)"), -5.0, 1.0);
}

TEST_F(LhsTest, LhsnormCovApprox)
{
    eval(R"(
        S = [1 0.5; 0.5 1];
        Y = lhsnorm([0 0], S, 1000);
        C = cov(Y);
        d11 = abs(C(1,1) - 1);
        d22 = abs(C(2,2) - 1);
        d12 = abs(C(1,2) - 0.5);
    )");
    EXPECT_LT(evalScalar("d11"), 0.2);
    EXPECT_LT(evalScalar("d22"), 0.2);
    EXPECT_LT(evalScalar("d12"), 0.15);
}

TEST_F(LhsTest, LhsnormShapeMismatchThrows)
{
    EXPECT_THROW(eval("lhsnorm([1 2 3], [1 0; 0 1], 5);"), std::exception);
}

// ── Options: smooth, criterion, iterations ──────────────────────────

TEST_F(LhsTest, LhsdesignSmoothOffMidpoints)
{
    // smooth='off' uses (perm[i] - 0.5)/n midpoints.
    // For n=5 each entry must round-trip to k - 0.5 with k integer.
    eval(R"(
        X = lhsdesign(5, 3, 'Smooth', 'off');
        vals = X(:) * 5 + 0.5;
        err = max(abs(vals - round(vals)));
    )");
    EXPECT_LT(evalScalar("err"), 1e-12);
}

TEST_F(LhsTest, LhsdesignMaximinBetterThanNone)
{
    // Maximin should give a design with min-pairwise-dist² no worse
    // than the first random LHS (criterion='none') given enough iters.
    eval(R"(
        rng(0); X_m = lhsdesign(20, 3, 'Criterion', 'maximin', 'Iterations', 5);
        rng(0); X_n = lhsdesign(20, 3, 'Criterion', 'none');
        function d2 = mind2(X)
            n = size(X, 1); d2 = Inf;
            for i = 1:n
                for k = i+1:n
                    s = sum((X(i,:) - X(k,:)).^2);
                    if s < d2; d2 = s; end
                end
            end
        end
        d_m = mind2(X_m);
        d_n = mind2(X_n);
    )");
    EXPECT_GE(evalScalar("d_m"), evalScalar("d_n"));
}

TEST_F(LhsTest, LhsdesignCorrelationCriterion)
{
    // 'correlation' should give a design with smaller max |corr| than
    // 'none' over the same trials.
    eval(R"(
        rng(0); Xc = lhsdesign(30, 4, 'Criterion', 'correlation', 'Iterations', 10);
        rng(0); Xn = lhsdesign(30, 4, 'Criterion', 'none');
        cc = corrcoef(Xc); cc(logical(eye(4))) = 0; max_c = max(abs(cc(:)));
        nn = corrcoef(Xn); nn(logical(eye(4))) = 0; max_n = max(abs(nn(:)));
    )");
    EXPECT_LE(evalScalar("max_c"), evalScalar("max_n") + 1e-12);
}

TEST_F(LhsTest, LhsdesignBinPartitionPreservedUnderMaximin)
{
    // Maximin criterion still produces a valid LHS — each column has
    // exactly one sample in each [(k-1)/n, k/n] bin.
    eval(R"(
        n = 10;
        X = lhsdesign(n, 3, 'Criterion', 'maximin', 'Iterations', 5);
        bins = ceil(X * n);
        ok = 1;
        for j = 1:3
            s = sort(bins(:, j))';
            if any(s ~= 1:n); ok = 0; end
        end
    )");
    EXPECT_EQ(static_cast<int>(evalScalar("ok")), 1);
}

TEST_F(LhsTest, LhsdesignBadCriterionThrows)
{
    EXPECT_THROW(eval("lhsdesign(5, 2, 'Criterion', 'badname');"), std::exception);
}
