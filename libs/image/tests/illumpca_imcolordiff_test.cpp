// libs/image/tests/illumpca_imcolordiff_test.cpp
//
// Regression guard for illumpca (Cheng-Prasad-Brown PCA white-balance)
// and imcolordiff (CIE94 + CIEDE2000 colour difference). Reference
// values from MATLAB R2025b verified bit-equal at 1e-12 for Lab-input
// formulas and at ~1e-6 for RGB-input formulas (limited by rgb2lab
// precision).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

#include <cmath>

using namespace numkit;

class IllumPcaImcdTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override {
        engine.eval("import compat.*;"
                    "A = zeros(10, 10, 3);"
                    "for i = 1:10;"
                    "  for j = 1:10;"
                    "    A(i,j,1) = 0.01 * (10*(i-1) + (j-1));"
                    "    A(i,j,2) = 0.01 * (10*(j-1) + (i-1));"
                    "    A(i,j,3) = 1.0 - A(i,j,1);"
                    "  end;"
                    "end;"
                    "I1 = [0.5 0.5 0.5; 0.8 0.2 0.3; 0.1 0.9 0.4];"
                    "I2 = [0.5 0.5 0.5; 0.7 0.3 0.4; 0.2 0.8 0.5];"
                    "L1 = [50 0 0; 60 5 -5; 70 -10 20];"
                    "L2 = [55 1 -1; 62 4 -6; 68 -12 22];");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── illumpca ────────────────────────────────────────────────────────

TEST_F(IllumPcaImcdTest, IllumpcaDefaultP35)
{
    eval("v = illumpca(A);");
    EXPECT_NEAR(evalScalar("v(1)"), 6.6707650155185549e-01, 1e-12);
    EXPECT_NEAR(evalScalar("v(2)"), 7.1629792796917069e-01, 1e-12);
    EXPECT_NEAR(evalScalar("v(3)"), 2.0475893012127530e-01, 1e-12);
}

TEST_F(IllumPcaImcdTest, IllumpcaP1)
{
    eval("v = illumpca(A, 1);");
    EXPECT_NEAR(evalScalar("v(1)"), 7.0703164163895937e-01, 1e-12);
    EXPECT_NEAR(evalScalar("v(2)"), 7.0703164163895937e-01, 1e-12);
    EXPECT_NEAR(evalScalar("v(3)"), 1.4577909405536311e-02, 1e-12);
}

TEST_F(IllumPcaImcdTest, IllumpcaP10)
{
    eval("v = illumpca(A, 10);");
    EXPECT_NEAR(evalScalar("v(1)"), 5.4017439688033231e-01, 1e-12);
    EXPECT_NEAR(evalScalar("v(2)"), 6.8126461213553013e-01, 1e-12);
    EXPECT_NEAR(evalScalar("v(3)"), 4.9405480384952760e-01, 1e-12);
}

TEST_F(IllumPcaImcdTest, IllumpcaP50UsesAll)
{
    eval("v = illumpca(A, 50);");
    EXPECT_NEAR(evalScalar("v(1)"), 5.6582242155465379e-01, 1e-12);
    EXPECT_NEAR(evalScalar("v(2)"), 6.1741361669061412e-01, 1e-12);
    EXPECT_NEAR(evalScalar("v(3)"), 5.4648459556609918e-01, 1e-12);
}

TEST_F(IllumPcaImcdTest, IllumpcaMask)
{
    eval("M = true(10, 10); M(1:5, :) = false;"
         "v = illumpca(A, 3.5, 'Mask', M);");
    EXPECT_NEAR(evalScalar("v(1)"), 7.4950269142921289e-01, 1e-12);
    EXPECT_NEAR(evalScalar("v(2)"), 6.5196171369337619e-01, 1e-12);
    EXPECT_NEAR(evalScalar("v(3)"), 1.1485486240626597e-01, 1e-12);
}

TEST_F(IllumPcaImcdTest, IllumpcaShape1x3)
{
    eval("v = illumpca(A);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(v,1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(v,2)")), 3);
}

TEST_F(IllumPcaImcdTest, IllumpcaPOutOfRangeThrows)
{
    EXPECT_THROW(eval("illumpca(A, 0);"),    std::exception);
    EXPECT_THROW(eval("illumpca(A, -1);"),   std::exception);
    EXPECT_THROW(eval("illumpca(A, 51);"),   std::exception);
}

TEST_F(IllumPcaImcdTest, IllumpcaBadShapeThrows)
{
    EXPECT_THROW(eval("illumpca(zeros(5, 5));"),   std::exception);
    EXPECT_THROW(eval("illumpca(zeros(5, 5, 4));"), std::exception);
}

// ── imcolordiff ─────────────────────────────────────────────────────

TEST_F(IllumPcaImcdTest, ImcolordiffCIE94RgbInput)
{
    eval("v = imcolordiff(I1, I2);");
    EXPECT_NEAR(evalScalar("v(1)"),  0.0,    1e-12);
    EXPECT_NEAR(evalScalar("v(2)"),  7.8188, 1e-3);
    EXPECT_NEAR(evalScalar("v(3)"), 10.0046, 1e-3);
}

TEST_F(IllumPcaImcdTest, ImcolordiffCIEDE2000RgbInput)
{
    eval("v = imcolordiff(I1, I2, 'Standard', 'CIEDE2000');");
    EXPECT_NEAR(evalScalar("v(1)"), 0.0,    1e-12);
    EXPECT_NEAR(evalScalar("v(2)"), 8.5052, 1e-3);
    EXPECT_NEAR(evalScalar("v(3)"), 8.7544, 1e-3);
}

TEST_F(IllumPcaImcdTest, ImcolordiffCIE94LabInput)
{
    eval("v = imcolordiff(L1, L2, 'isInputLab', true);");
    EXPECT_NEAR(evalScalar("v(1)"), 5.1961524227066320, 1e-9);
    EXPECT_NEAR(evalScalar("v(2)"), 2.3727765217124466, 1e-7);
    EXPECT_NEAR(evalScalar("v(3)"), 2.4921072619931817, 1e-7);
}

TEST_F(IllumPcaImcdTest, ImcolordiffCIEDE2000LabInput)
{
    eval("v = imcolordiff(L1, L2, 'isInputLab', true, 'Standard', 'CIEDE2000');");
    EXPECT_NEAR(evalScalar("v(1)"), 5.2068379556321300, 1e-7);
    EXPECT_NEAR(evalScalar("v(2)"), 2.3354751788587089, 1e-7);
    EXPECT_NEAR(evalScalar("v(3)"), 2.2025135169780774, 1e-7);
}

TEST_F(IllumPcaImcdTest, ImcolordiffKWeights)
{
    // Custom kL — scales the L*-difference contribution. Equal Lab
    // inputs ⇒ result still 0 regardless of weights.
    eval("v = imcolordiff(L1, L1, 'isInputLab', true, 'kL', 2);");
    EXPECT_DOUBLE_EQ(evalScalar("v(1)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(2)"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("v(3)"), 0.0);
}

TEST_F(IllumPcaImcdTest, ImcolordiffShapeColormap)
{
    eval("v = imcolordiff(I1, I2);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(v,1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(v,2)")), 1);
}

TEST_F(IllumPcaImcdTest, ImcolordiffShapeImage)
{
    eval("I1img = reshape(I1, 1, 3, 3);"
         "I2img = reshape(I2, 1, 3, 3);"
         "v = imcolordiff(I1img, I2img);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(v,1)")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("size(v,2)")), 3);
}

TEST_F(IllumPcaImcdTest, ImcolordiffBadStandardThrows)
{
    EXPECT_THROW(eval("imcolordiff(I1, I2, 'Standard', 'CIE76');"),
                 std::exception);
}

TEST_F(IllumPcaImcdTest, ImcolordiffBadWeightThrows)
{
    EXPECT_THROW(eval("imcolordiff(I1, I2, 'kL', -1);"), std::exception);
    EXPECT_THROW(eval("imcolordiff(I1, I2, 'K1', 0);"),  std::exception);
}

TEST_F(IllumPcaImcdTest, ImcolordiffMismatchedShapesThrows)
{
    EXPECT_THROW(eval("imcolordiff(I1, [0.5 0.5 0.5]);"), std::exception);
    EXPECT_THROW(eval("imcolordiff(zeros(2,3,3), zeros(3,2,3));"),
                 std::exception);
}

TEST_F(IllumPcaImcdTest, ImcolordiffUnknownNvThrows)
{
    EXPECT_THROW(eval("imcolordiff(I1, I2, 'NotAnOption', 1);"),
                 std::exception);
}
