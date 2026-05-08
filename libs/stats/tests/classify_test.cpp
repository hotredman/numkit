// libs/stats/tests/classify_test.cpp
// Audit ТЗ closure for classify. Closes audit/findings/lda/classify.md.

#include <numkit/builtin/library.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ClassifyTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override {
        engine.eval("import compat.*;");
        engine.eval("training = [1 1; 1 2; 2 1; 2 2; "
                    "5 5; 5 6; 6 5; 6 6; "
                    "9 0; 10 0; 10 1; 9 1];");
        engine.eval("group = [1 1 1 1 2 2 2 2 3 3 3 3]';");
        engine.eval("sample = [1.5 1.5; 5.5 5.5; 9.5 0.5; 3 3];");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(ClassifyTest, LinearDefault)
{
    eval("[c, err, post, logp] = classify(sample, training, group);");
    EXPECT_DOUBLE_EQ(evalScalar("c(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(3)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(4)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("err"), 0.0);
    EXPECT_NEAR(evalScalar("logp(1)"), -1.83788, 1e-5);
    EXPECT_NEAR(evalScalar("post(1, 1)"), 1.0, 1e-9);
    EXPECT_NEAR(evalScalar("post(4, 1)"), 0.999994, 1e-5);
}

TEST_F(ClassifyTest, Quadratic)
{
    eval("[c, err] = classify(sample, training, group, 'quadratic');");
    EXPECT_DOUBLE_EQ(evalScalar("c(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(3)"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("err"), 0.0);
}

TEST_F(ClassifyTest, DiagLinear)
{
    eval("c = classify(sample, training, group, 'diaglinear');");
    EXPECT_DOUBLE_EQ(evalScalar("c(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(3)"), 3.0);
}

TEST_F(ClassifyTest, DiagQuadratic)
{
    eval("c = classify(sample, training, group, 'diagquadratic');");
    EXPECT_DOUBLE_EQ(evalScalar("c(1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(2)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("c(3)"), 3.0);
}

TEST_F(ClassifyTest, MahalanobisRejected)
{
    bool threw = false;
    try { eval("classify(sample, training, group, 'mahalanobis');"); }
    catch (const std::exception &) { threw = true; }
    EXPECT_TRUE(threw);
}
