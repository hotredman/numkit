// toolboxes/image/tests/imnlmfilt_test.cpp
//
// Regression guard for imnlmfilt — Non-Local Means denoising
// (Buades-Coll-Morel 2005). MATLAB R2025b parity at 1e-4
// (boundary positions; centre matches at 1e-9).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ImnlmfiltTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {
        engine.eval(

            "I = double(reshape(1:441, 21, 21)) / 441;");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── Default args (centre pixel — bit-exact) ──────────────────────

TEST_F(ImnlmfiltTest, DefaultCentre)
{
    eval("[B, estDoS] = imnlmfilt(I);");
    EXPECT_NEAR(evalScalar("B(11,11)"), 0.5011337868, 1e-9);
}

// ── Estimated DegreeOfSmoothing (Immerkaer 1996) ─────────────────

TEST_F(ImnlmfiltTest, EstimatedDoS)
{
    eval("[B, estDoS] = imnlmfilt(I);");
    EXPECT_NEAR(evalScalar("estDoS"), 0.004747144082, 1e-7);
}

// ── Custom DegreeOfSmoothing ────────────────────────────────────

TEST_F(ImnlmfiltTest, CustomDoS)
{
    eval("B = imnlmfilt(I, 'DegreeOfSmoothing', 0.05);");
    EXPECT_NEAR(evalScalar("B(11,11)"), 0.5011337868, 1e-9);
}

// ── Custom ComparisonWindowSize ─────────────────────────────────

TEST_F(ImnlmfiltTest, CustomCWS)
{
    eval("B = imnlmfilt(I, 'ComparisonWindowSize', 3);");
    EXPECT_NEAR(evalScalar("B(11,11)"), 0.5011337868, 1e-9);
}

// ── Custom SearchWindowSize ─────────────────────────────────────

TEST_F(ImnlmfiltTest, CustomSWS)
{
    eval("B = imnlmfilt(I, 'SearchWindowSize', 11);");
    EXPECT_NEAR(evalScalar("B(11,11)"), 0.5011337868, 1e-9);
}

// ── uint8 input class preserved ─────────────────────────────────

TEST_F(ImnlmfiltTest, Uint8Class)
{
    eval("Bu = imnlmfilt(uint8(I*255));");
    EXPECT_EQ(static_cast<int>(evalScalar("double(Bu(11,11))")), 128);
    EXPECT_EQ(eval("class(Bu)").toString(), "uint8");
}

// ── Output shape preserved ──────────────────────────────────────

TEST_F(ImnlmfiltTest, ShapePreserved)
{
    eval("B = imnlmfilt(I);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B, 1)")), 21);
    EXPECT_EQ(static_cast<int>(evalScalar("size(B, 2)")), 21);
}

// ── Errors ──────────────────────────────────────────────────────

TEST_F(ImnlmfiltTest, ImageTooSmallThrows)
{
    EXPECT_THROW(eval("imnlmfilt(zeros(20, 20));"), std::exception);
}

TEST_F(ImnlmfiltTest, EvenSWSThrows)
{
    EXPECT_THROW(eval("imnlmfilt(I, 'SearchWindowSize', 8);"),
                 std::exception);
}

TEST_F(ImnlmfiltTest, CWSExceedsSWSThrows)
{
    EXPECT_THROW(eval("imnlmfilt(I, 'ComparisonWindowSize', 7, "
                      "'SearchWindowSize', 5);"),
                 std::exception);
}
