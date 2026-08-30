// wavedec2_test.cpp — 2-D multilevel DWT family (wavedec2 / waverec2 /
// appcoef2 / detcoef2).
//
// Built on the single-level dwt2/idwt2 (the 2-D analogue of how wavedec
// wraps dwt). [C,S] layout is MATLAB-canonical. Expected values verified
// vs MATLAB R2025b. Fixes bugs/wavelet/wavedec2-family.md.

#include <numkit/bundle/standard_engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class Wavedec2Test : public ::testing::Test {
public:
    numkit::StandardEngine engine;
    void SetUp() override {}
    numkit::Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// Single-level db1 (Haar) on a 4x4 ramp — full [C,S] + extractors.
TEST_F(Wavedec2Test, Level1_Db1) {
    eval("[c, s] = wavedec2(reshape(1:16,4,4), 1, 'db1');");
    EXPECT_EQ((int)evalScalar("numel(c)"), 16);
    EXPECT_NEAR(evalScalar("c(1)"), 7.0, 1e-10);
    // S = [2 2; 2 2; 4 4]
    EXPECT_EQ((int)evalScalar("s(1,1)"), 2);
    EXPECT_EQ((int)evalScalar("s(1,2)"), 2);
    EXPECT_EQ((int)evalScalar("s(3,1)"), 4);
    eval("H = detcoef2('h', c, s, 1); V = detcoef2('v', c, s, 1); D = detcoef2('d', c, s, 1);");
    EXPECT_NEAR(evalScalar("H(1,1)"), -1.0, 1e-10);
    EXPECT_NEAR(evalScalar("V(1,1)"), -4.0, 1e-10);
    EXPECT_NEAR(evalScalar("D(1,1)"),  0.0, 1e-10);
    EXPECT_EQ((int)evalScalar("size(H,1)"), 2);
    eval("A = appcoef2(c, s, 'db1', 1);");
    EXPECT_NEAR(evalScalar("A(1,1)"),  7.0, 1e-10);
    EXPECT_NEAR(evalScalar("A(2,2)"), 27.0, 1e-10);
}

// Two-level db2 on an 8x8 ramp — non-trivial filter, level-2 approx + level-1 detail.
TEST_F(Wavedec2Test, Level2_Db2) {
    eval("[c, s] = wavedec2(reshape(1:64,8,8), 2, 'db2');");
    EXPECT_EQ((int)evalScalar("numel(c)"), 139);
    // S = [4 4; 4 4; 5 5; 8 8]
    EXPECT_EQ((int)evalScalar("s(1,1)"), 4);
    EXPECT_EQ((int)evalScalar("s(3,1)"), 5);
    EXPECT_EQ((int)evalScalar("s(4,1)"), 8);
    eval("A2 = appcoef2(c, s, 'db2', 2);");
    EXPECT_NEAR(evalScalar("A2(1,1)"), 16.4557713660, 1e-7);
    EXPECT_EQ((int)evalScalar("size(A2,1)"), 4);
    eval("H1 = detcoef2('h', c, s, 1);");
    EXPECT_NEAR(evalScalar("H1(1,1)"), -0.8660254038, 1e-7);
    EXPECT_EQ((int)evalScalar("size(H1,1)"), 5);
}

// detcoef2('all', …) returns H, V, D in one call.
TEST_F(Wavedec2Test, Detcoef2All) {
    eval("[c, s] = wavedec2(reshape(1:16,4,4), 1, 'db1'); [H, V, D] = detcoef2('all', c, s, 1);");
    EXPECT_NEAR(evalScalar("H(1,1)"), -1.0, 1e-10);
    EXPECT_NEAR(evalScalar("V(1,1)"), -4.0, 1e-10);
    EXPECT_NEAR(evalScalar("D(1,1)"),  0.0, 1e-10);
}

// waverec2 perfectly reconstructs (db2, two levels).
TEST_F(Wavedec2Test, Waverec2RoundTrip) {
    eval("X = reshape(1:64,8,8); [c, s] = wavedec2(X, 2, 'db2'); R = waverec2(c, s, 'db2');");
    EXPECT_LT(evalScalar("max(max(abs(R - X)))"), 1e-9);
}

// Non-square image (5x3), single level.
TEST_F(Wavedec2Test, NonSquare) {
    eval("Z = reshape(1:15,5,3); [c, s] = wavedec2(Z, 1, 'db1');");
    EXPECT_EQ((int)evalScalar("numel(c)"), 24);
    EXPECT_NEAR(evalScalar("c(1)"), 8.0, 1e-10);
    EXPECT_EQ((int)evalScalar("s(1,1)"), 3);   // ceil(5/2)
    EXPECT_EQ((int)evalScalar("s(1,2)"), 2);   // ceil(3/2)
    eval("R = waverec2(c, s, 'db1');");
    EXPECT_LT(evalScalar("max(max(abs(R - Z)))"), 1e-10);
}

// Biorthogonal wavelet works in 2-D too (distinct analysis/synthesis filters).
TEST_F(Wavedec2Test, Biorthogonal2D) {
    eval("X = reshape(1:16,4,4); [c, s] = wavedec2(X, 1, 'bior2.2'); R = waverec2(c, s, 'bior2.2');");
    EXPECT_LT(evalScalar("max(max(abs(R - X)))"), 1e-10);
}

// appcoef2 default level (no level arg) returns the coarsest approximation.
TEST_F(Wavedec2Test, AppcoefDefaultLevel) {
    eval("X = reshape(1:64,8,8); [c, s] = wavedec2(X, 2, 'db2');");
    eval("Adef = appcoef2(c, s, 'db2'); A2 = appcoef2(c, s, 'db2', 2);");
    EXPECT_LT(evalScalar("max(max(abs(Adef - A2)))"), 1e-12);
}
