// toolboxes/image/tests/raw_planar_test.cpp
//
// Regression guard for raw2planar / planar2raw — Bayer CFA mosaic
// ↔ 4-plane sensor-element deinterleave.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class RawPlanarTest : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override {}
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── raw2planar core behaviour ─────────────────────────────────────

TEST_F(RawPlanarTest, Uint8Reshape64Layout)
{
    eval("I = uint8(reshape(1:64, 8, 8));"
         "P = raw2planar(I);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(P,1)")), 4);
    EXPECT_EQ(static_cast<int>(evalScalar("size(P,2)")), 4);
    EXPECT_EQ(static_cast<int>(evalScalar("size(P,3)")), 4);
    // (col-major reshape 1:64 → A(1,1)=1, A(2,1)=2, A(1,2)=9, A(2,2)=10)
    EXPECT_EQ(static_cast<int>(evalScalar("double(P(1,1,1))")),  1);
    EXPECT_EQ(static_cast<int>(evalScalar("double(P(1,1,2))")),  9);
    EXPECT_EQ(static_cast<int>(evalScalar("double(P(1,1,3))")),  2);
    EXPECT_EQ(static_cast<int>(evalScalar("double(P(1,1,4))")), 10);
    EXPECT_EQ(static_cast<int>(evalScalar("double(P(2,2,1))")), 19);
    EXPECT_EQ(static_cast<int>(evalScalar("double(P(2,2,2))")), 27);
    EXPECT_EQ(static_cast<int>(evalScalar("double(P(2,2,3))")), 20);
    EXPECT_EQ(static_cast<int>(evalScalar("double(P(2,2,4))")), 28);
}

TEST_F(RawPlanarTest, DistinguishablePattern)
{
    eval("I = uint8(zeros(8,8));"
         "I(1:2:end,1:2:end) = 100;"
         "I(1:2:end,2:2:end) = 50;"
         "I(2:2:end,1:2:end) = 60;"
         "I(2:2:end,2:2:end) = 200;"
         "P = raw2planar(I);");
    // All cells in plane 1 should be 100, plane 2 = 50, etc.
    EXPECT_EQ(static_cast<int>(evalScalar("double(P(1,1,1))")), 100);
    EXPECT_EQ(static_cast<int>(evalScalar("double(P(1,1,2))")),  50);
    EXPECT_EQ(static_cast<int>(evalScalar("double(P(1,1,3))")),  60);
    EXPECT_EQ(static_cast<int>(evalScalar("double(P(1,1,4))")), 200);
    EXPECT_EQ(static_cast<int>(evalScalar("double(P(4,4,1))")), 100);
    EXPECT_EQ(static_cast<int>(evalScalar("double(P(4,4,4))")), 200);
}

// ── class preservation ────────────────────────────────────────────

TEST_F(RawPlanarTest, Uint16ClassPreserved)
{
    eval("P = raw2planar(uint16(reshape(1:64, 8, 8)));");
    EXPECT_EQ(eval("class(P)").toString(), "uint16");
}

TEST_F(RawPlanarTest, DoubleAccepted)
{
    eval("P = raw2planar(double(reshape(1:36, 6, 6)));");
    EXPECT_EQ(eval("class(P)").toString(), "double");
    EXPECT_EQ(static_cast<int>(evalScalar("size(P,1)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(P,2)")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("size(P,3)")), 4);
}

TEST_F(RawPlanarTest, LogicalAccepted)
{
    eval("P = raw2planar(logical(mod(reshape(1:64, 8, 8), 2)));");
    EXPECT_EQ(eval("class(P)").toString(), "logical");
}

// ── round trip ────────────────────────────────────────────────────

TEST_F(RawPlanarTest, RoundTripUint8)
{
    eval("I = uint8(reshape(1:64, 8, 8));"
         "back = planar2raw(raw2planar(I));");
    EXPECT_EQ(static_cast<int>(evalScalar("double(isequal(I, back))")), 1);
}

TEST_F(RawPlanarTest, RoundTripUint16)
{
    eval("I = uint16(reshape(1:100, 10, 10));"
         "back = planar2raw(raw2planar(I));");
    EXPECT_EQ(static_cast<int>(evalScalar("double(isequal(I, back))")), 1);
}

// ── planar2raw basic ─────────────────────────────────────────────

TEST_F(RawPlanarTest, Planar2RawBasicShape)
{
    eval("P = uint8(zeros(3,3,4));"
         "P(:,:,1) = 100; P(:,:,2) = 50; P(:,:,3) = 60; P(:,:,4) = 200;"
         "cfa = planar2raw(P);");
    EXPECT_EQ(static_cast<int>(evalScalar("size(cfa,1)")), 6);
    EXPECT_EQ(static_cast<int>(evalScalar("size(cfa,2)")), 6);
    EXPECT_EQ(static_cast<int>(evalScalar("double(cfa(1,1))")), 100);
    EXPECT_EQ(static_cast<int>(evalScalar("double(cfa(1,2))")),  50);
    EXPECT_EQ(static_cast<int>(evalScalar("double(cfa(2,1))")),  60);
    EXPECT_EQ(static_cast<int>(evalScalar("double(cfa(2,2))")), 200);
}

// ── errors ────────────────────────────────────────────────────────

TEST_F(RawPlanarTest, OddDimThrows)
{
    EXPECT_THROW(eval("raw2planar(uint8(zeros(5,4)));"), std::exception);
}

TEST_F(RawPlanarTest, Planar2RawBadShapeThrows)
{
    EXPECT_THROW(eval("planar2raw(uint8(zeros(4,4,3)));"), std::exception);
}
