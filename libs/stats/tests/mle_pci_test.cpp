// libs/stats/tests/mle_pci_test.cpp
//
// Regression guard for bugs/stats/mle-output.md (FIXED): [phat, pci] = mle(...)
// emits the 2nd output pci (a 2×k confidence-interval matrix, row 1 = lower,
// row 2 = upper, one column per parameter), reusing the matching *fit CI
// machinery. Default Alpha = 0.05; an 'Alpha' option is honoured. MATLAB R2025b.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class MlePciTest : public ::testing::Test
{
public:
    numkit::StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    numkit::Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Default (normal): pci is 2x2, columns = mu CI / sigma CI.
TEST_F(MlePciTest, NormalDefault)
{
    eval("[phat, pci] = mle([2 3 4 5 6 4 3]);");
    EXPECT_NEAR(evalScalar("phat(1)"), 3.857143, 1e-5);
    EXPECT_NEAR(evalScalar("phat(2)"), 1.245400, 1e-5);    // MLE sigma (divisor N)
    EXPECT_EQ(static_cast<int>(evalScalar("size(pci,1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(pci,2)")), 2);
    EXPECT_NEAR(evalScalar("pci(1,1)"), 2.613054, 1e-5);   // mu lower
    EXPECT_NEAR(evalScalar("pci(2,1)"), 5.101232, 1e-5);   // mu upper
    EXPECT_NEAR(evalScalar("pci(1,2)"), 0.866829, 1e-5);   // sigma lower
    EXPECT_NEAR(evalScalar("pci(2,2)"), 2.962187, 1e-5);   // sigma upper
}

// Exponential: pci is 2x1.
TEST_F(MlePciTest, Exponential)
{
    eval("[phat, pci] = mle([1.2 0.5 2.1 0.8 3.0 1.5], 'distribution', 'exp');");
    EXPECT_NEAR(evalScalar("phat"), 1.516667, 1e-5);
    EXPECT_EQ(static_cast<int>(evalScalar("size(pci,1)")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("size(pci,2)")), 1);
    EXPECT_NEAR(evalScalar("pci(1)"), 0.779889, 1e-5);
    EXPECT_NEAR(evalScalar("pci(2)"), 4.132805, 1e-5);
}

// Poisson: pci is 2x1.
TEST_F(MlePciTest, Poisson)
{
    eval("[phat, pci] = mle([2 3 1 4 2 5 3], 'distribution', 'poisson');");
    EXPECT_NEAR(evalScalar("phat"), 2.857143, 1e-5);
    EXPECT_NEAR(evalScalar("pci(1)"), 1.745217, 1e-5);
    EXPECT_NEAR(evalScalar("pci(2)"), 4.412625, 1e-5);
}

// Lognormal: pci on (mu, sigma) of log(data).
TEST_F(MlePciTest, Lognormal)
{
    eval("[phat, pci] = mle([1.2 2.5 0.8 3.1 1.9 4.2 2.0], 'distribution', 'lognormal');");
    EXPECT_NEAR(evalScalar("pci(1,1)"), 0.162710, 1e-5);
    EXPECT_NEAR(evalScalar("pci(2,1)"), 1.202134, 1e-5);
    EXPECT_NEAR(evalScalar("pci(1,2)"), 0.362113, 1e-5);
    EXPECT_NEAR(evalScalar("pci(2,2)"), 1.237439, 1e-5);
}

// Alpha option widens the interval.
TEST_F(MlePciTest, AlphaOption)
{
    eval("[~, pci] = mle([2 3 4 5 6 4 3], 'Alpha', 0.01);");
    EXPECT_NEAR(evalScalar("pci(1,1)"), 1.972167, 1e-5);   // wider than 0.05 (2.613)
    EXPECT_ANY_THROW(eval("mle([2 3 4 5 6 4 3], 'Alpha', 1.5);"));
}

// The 1-output form is unchanged (no pci computed).
TEST_F(MlePciTest, OneOutputUnchanged)
{
    eval("phat = mle([2 3 4 5 6 4 3]);");
    EXPECT_NEAR(evalScalar("phat(1)"), 3.857143, 1e-5);
    EXPECT_EQ(static_cast<int>(evalScalar("numel(phat)")), 2);
}
