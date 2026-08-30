// biorfilt_test.cpp — biorthogonal (bior*) and reverse-biorthogonal
// (rbio*) wavelet families.
//
// Biorthogonal wavelets have distinct analysis/synthesis filter pairs
// (Lo_D/Hi_D != Lo_R/Hi_R), tabulated in filter/biorfilt.cpp. The
// dwt/idwt/wavedec/waverec machinery already threads all four filters, so
// adding the tables enables the whole family. Expected values are the
// Cohen-Daubechies-Feauveau coefficients, verified vs MATLAB R2025b.
// Fixes bugs/wavelet/dwt-biorthogonal.md.

#include <numkit/bundle/standard_engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class BiorfiltTest : public ::testing::Test {
public:
    numkit::StandardEngine engine;
    void SetUp() override {}
    numkit::Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// wfilters returns the four distinct bior2.2 filters (len 6, zero-padded).
TEST_F(BiorfiltTest, Wfilters_Bior22) {
    eval("[LoD, HiD, LoR, HiR] = wfilters('bior2.2');");
    EXPECT_EQ((int)evalScalar("numel(LoD)"), 6);
    EXPECT_NEAR(evalScalar("LoD(2)"), -0.17677669529663689, 1e-15);
    EXPECT_NEAR(evalScalar("LoD(4)"),  1.0606601717798214,  1e-15);
    EXPECT_NEAR(evalScalar("LoR(3)"),  0.70710678118654757, 1e-15);
    EXPECT_NEAR(evalScalar("HiR(4)"), -1.0606601717798214,  1e-15);
    // analysis != synthesis lowpass (the defining biorthogonal property).
    EXPECT_GT(evalScalar("max(abs(LoD - LoR))"), 0.5);
}

// bior4.4 (JPEG2000 9/7-class) — len 10, distinct pairs.
TEST_F(BiorfiltTest, Wfilters_Bior44) {
    eval("[LoD, HiD, LoR, HiR] = wfilters('bior4.4');");
    EXPECT_EQ((int)evalScalar("numel(LoD)"), 10);
    EXPECT_NEAR(evalScalar("LoD(6)"),  0.85269867900889385, 1e-15);
    EXPECT_NEAR(evalScalar("HiD(2)"), -0.064538882628697058, 1e-15);
}

// dwt with a biorthogonal wavelet — analysis coefficients vs MATLAB.
TEST_F(BiorfiltTest, Dwt_Bior22) {
    eval("[a, d] = dwt([1 2 3 4 5 6 7 8], 'bior2.2');");
    EXPECT_NEAR(evalScalar("a(1)"), 2.65165042944955, 1e-12);
    EXPECT_NEAR(evalScalar("a(2)"), 1.23743686707646, 1e-12);
    EXPECT_NEAR(evalScalar("d(1)"), 0.353553390593274, 1e-12);
}

TEST_F(BiorfiltTest, Dwt_Bior44) {
    eval("[a, d] = dwt([1 2 3 4 5 6 7 8], 'bior4.4');");
    EXPECT_NEAR(evalScalar("a(1)"),  5.69468270499965,   1e-12);
    EXPECT_NEAR(evalScalar("d(1)"), -0.0645388826329399, 1e-12);
    EXPECT_NEAR(evalScalar("d(2)"),  0.21746611290421,   1e-12);
}

TEST_F(BiorfiltTest, Dwt_Rbio33) {
    eval("[a, d] = dwt([1 2 3 4 5 6 7 8], 'rbio3.3');");
    EXPECT_NEAR(evalScalar("a(1)"),  3.53553390593274,  1e-12);
    EXPECT_NEAR(evalScalar("d(1)"), -0.397747564417433, 1e-12);
}

// Perfect reconstruction: idwt(dwt(x)) == x.
TEST_F(BiorfiltTest, RoundTrip_Idwt) {
    eval("x = [1 2 3 4 5 6 7 8]; [a, d] = dwt(x, 'bior2.2'); r = idwt(a, d, 'bior2.2');");
    EXPECT_LT(evalScalar("max(abs(r - x))"), 1e-12);
    eval("[a4, d4] = dwt(x, 'bior4.4'); r4 = idwt(a4, d4, 'bior4.4');");
    EXPECT_LT(evalScalar("max(abs(r4 - x))"), 1e-10);
}

// Multilevel wavedec / waverec round-trip with a biorthogonal wavelet.
TEST_F(BiorfiltTest, Wavedec_Waverec_Bior22) {
    eval("x = [1 2 3 4 5 6 7 8]; [C, L] = wavedec(x, 2, 'bior2.2');");
    EXPECT_NEAR(evalScalar("C(1)"), 2.03125, 1e-10);
    EXPECT_NEAR(evalScalar("C(2)"), 3.21875, 1e-10);
    EXPECT_NEAR(evalScalar("C(3)"), 5.1875,  1e-10);
    EXPECT_EQ((int)evalScalar("L(1)"), 5);   // approx length at level 2
    eval("xr = waverec(C, L, 'bior2.2');");
    EXPECT_LT(evalScalar("max(abs(xr - x))"), 1e-10);
}

// bior1.1 is the Haar wavelet.
TEST_F(BiorfiltTest, Bior11IsHaar) {
    eval("[a, d] = dwt([1 2 3 4], 'bior1.1'); [ah, dh] = dwt([1 2 3 4], 'haar');");
    EXPECT_LT(evalScalar("max(abs(a - ah))"), 1e-14);
    EXPECT_LT(evalScalar("max(abs(d - dh))"), 1e-14);
}

// Unknown family still throws (the gate message now lists bior/rbio).
TEST_F(BiorfiltTest, UnknownFamilyThrows) {
    EXPECT_THROW(eval("wfilters('bior9.9');"), std::exception);
}
