// corner_test.cpp — corner-point detection (corner), built on cornermetric.
//
// corner(I) = cornermetric -> local maxima above QualityLevel*max ->
// connected-peak centroids -> strength-descending (ties column-major) ->
// up to N [x y]=[col row] integer coordinates. Verified vs MATLAB R2025b.
// Fixes bugs/image/corner.md.

#include <numkit/bundle/standard_engine.hpp>
#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class CornerTest : public ::testing::Test {
public:
    numkit::StandardEngine engine;
    void SetUp() override { engine.eval("import compat.*;"); }
    numkit::Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// A bright square block has exactly 4 corners, returned column-major.
TEST_F(CornerTest, SquareBlock) {
    eval("I = zeros(20,20); I(6:15,6:15) = 1; C = corner(I);");
    EXPECT_EQ((int)evalScalar("size(C,1)"), 4);
    EXPECT_EQ((int)evalScalar("size(C,2)"), 2);
    // MATLAB order: [6 6; 6 15; 15 6; 15 15]
    EXPECT_EQ((int)evalScalar("C(1,1)"), 6);  EXPECT_EQ((int)evalScalar("C(1,2)"), 6);
    EXPECT_EQ((int)evalScalar("C(2,1)"), 6);  EXPECT_EQ((int)evalScalar("C(2,2)"), 15);
    EXPECT_EQ((int)evalScalar("C(3,1)"), 15); EXPECT_EQ((int)evalScalar("C(3,2)"), 6);
    EXPECT_EQ((int)evalScalar("C(4,1)"), 15); EXPECT_EQ((int)evalScalar("C(4,2)"), 15);
}

// Two squares of different contrast: strong corners sort first.
TEST_F(CornerTest, StrengthOrdering) {
    eval("A = zeros(30,30); A(5:9,5:9) = 1; A(20:27,20:27) = 0.5; C = corner(A);");
    EXPECT_EQ((int)evalScalar("size(C,1)"), 8);
    // first 4 = strong square (low cols), then weak square.
    EXPECT_EQ((int)evalScalar("C(1,1)"), 5);  EXPECT_EQ((int)evalScalar("C(1,2)"), 5);
    EXPECT_EQ((int)evalScalar("C(4,1)"), 9);  EXPECT_EQ((int)evalScalar("C(4,2)"), 9);
    EXPECT_EQ((int)evalScalar("C(5,1)"), 20); EXPECT_EQ((int)evalScalar("C(5,2)"), 20);
}

// Strength sort wins over position: strong block at high columns comes first.
TEST_F(CornerTest, StrengthBeatsPosition) {
    eval("W = zeros(30,30); W(5:9,5:9) = 0.3; W(20:24,20:24) = 1.0; C = corner(W, 1);");
    EXPECT_EQ((int)evalScalar("size(C,1)"), 1);
    EXPECT_EQ((int)evalScalar("C(1,1)"), 20);   // the strong (high-col) corner
    EXPECT_EQ((int)evalScalar("C(1,2)"), 20);
}

// N limits the count, keeping the strongest (column-major within ties).
TEST_F(CornerTest, MaxCount) {
    eval("A = zeros(30,30); A(5:9,5:9) = 1; A(20:27,20:27) = 0.5; C = corner(A, 2);");
    EXPECT_EQ((int)evalScalar("size(C,1)"), 2);
    EXPECT_EQ((int)evalScalar("C(1,1)"), 5);
    EXPECT_EQ((int)evalScalar("C(2,2)"), 9);
}

// Border corners are excluded naturally (their metric is <= 0 < threshold).
TEST_F(CornerTest, BorderExcluded) {
    eval("B = zeros(20,20); B(1:6,1:6) = 1; C = corner(B);");
    EXPECT_EQ((int)evalScalar("size(C,1)"), 1);   // only the inner corner
    EXPECT_EQ((int)evalScalar("C(1,1)"), 6);
    EXPECT_EQ((int)evalScalar("C(1,2)"), 6);
}

// MinimumEigenvalue method also finds the 4 corners.
TEST_F(CornerTest, MinEigenvalueMethod) {
    eval("I = zeros(20,20); I(6:15,6:15) = 1; C = corner(I, 'MinimumEigenvalue');");
    EXPECT_EQ((int)evalScalar("size(C,1)"), 4);
}

// A flat image has no corners -> 0x2 result.
TEST_F(CornerTest, FlatImageEmpty) {
    eval("C = corner(zeros(10,10));");
    EXPECT_EQ((int)evalScalar("size(C,1)"), 0);
    EXPECT_EQ((int)evalScalar("size(C,2)"), 2);
}

// QualityLevel raises the acceptance threshold (here still keeps all 4 equal corners).
TEST_F(CornerTest, QualityLevel) {
    eval("I = zeros(20,20); I(6:15,6:15) = 1; C = corner(I, 'QualityLevel', 0.5);");
    EXPECT_EQ((int)evalScalar("size(C,1)"), 4);
}
