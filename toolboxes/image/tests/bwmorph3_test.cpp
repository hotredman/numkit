// toolboxes/image/tests/bwmorph3_test.cpp
//
// Regression guard for image/bwmorph3 — 3-D binary morphology.
// Fingerprints from MATLAB R2025b (clean-room port of bwmorph3Algorithm).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class Bwmorph3Test : public ::testing::Test
{
public:
    StandardEngine engine;
    void SetUp() override
    {
        engine.eval("import compat.*;");
        // 3x3x3 solid cube with the centre voxel removed (26 voxels set).
        engine.eval("V = false(5,5,5); V(2:4,2:4,2:4) = true; V(3,3,3) = false;");
        // A straight line of 5 voxels along the page (z) axis.
        engine.eval("L = false(5,5,7); L(3,3,2:6) = true;");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(Bwmorph3Test, BranchpointsCountGtThree)
{
    eval("J = bwmorph3(V, 'branchpoints');");
    EXPECT_TRUE(eval("J").type() == ValueType::LOGICAL);
    EXPECT_EQ(static_cast<int>(evalScalar("nnz(J)")), 26);   // dense blob, all >3
    // Spatial: a set cube-corner is a branchpoint; the hole is not.
    EXPECT_DOUBLE_EQ(evalScalar("double(J(2,2,2))"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("double(J(3,3,3))"), 0.0);   // the hole
    // A z-line has no branch points.
    EXPECT_EQ(static_cast<int>(evalScalar("nnz(bwmorph3(L,'branchpoints'))")), 0);
}

TEST_F(Bwmorph3Test, CleanRemovesIsolated)
{
    eval("J = bwmorph3(V, 'clean');");
    EXPECT_EQ(static_cast<int>(evalScalar("nnz(J)")), 26);   // none isolated
    eval("Iso = false(5,5,5); Iso(3,3,3) = true;");
    EXPECT_EQ(static_cast<int>(evalScalar("nnz(bwmorph3(Iso,'clean'))")), 0);
}

TEST_F(Bwmorph3Test, EndpointsCountEqTwo)
{
    EXPECT_EQ(static_cast<int>(evalScalar("nnz(bwmorph3(V,'endpoints'))")), 0);
    // z-line: the two ends each have exactly one neighbour.
    eval("E = bwmorph3(L,'endpoints');");
    EXPECT_EQ(static_cast<int>(evalScalar("nnz(E)")), 2);
    // Spatial: the endpoints are the two line ends, not interior voxels.
    EXPECT_DOUBLE_EQ(evalScalar("double(E(3,3,2))"), 1.0);  // first end
    EXPECT_DOUBLE_EQ(evalScalar("double(E(3,3,6))"), 1.0);  // last end
    EXPECT_DOUBLE_EQ(evalScalar("double(E(3,3,4))"), 0.0);  // interior
}

TEST_F(Bwmorph3Test, FillFillsInteriorHole)
{
    eval("J = bwmorph3(V, 'fill');");
    EXPECT_EQ(static_cast<int>(evalScalar("nnz(J)")), 27);   // hole filled
    EXPECT_DOUBLE_EQ(evalScalar("double(J(3,3,3))"), 1.0);
}

TEST_F(Bwmorph3Test, MajorityFourteenOf27)
{
    eval("J = bwmorph3(V, 'majority');");
    EXPECT_EQ(static_cast<int>(evalScalar("nnz(J)")), 7);
    // Centre's 27-neighbourhood holds 26 set voxels (>13) -> set.
    EXPECT_DOUBLE_EQ(evalScalar("double(J(3,3,3))"), 1.0);
}

TEST_F(Bwmorph3Test, RemoveStripsInterior)
{
    // A SOLID 3x3x3 cube exercises the strip path: the centre voxel has all
    // six face-neighbours set, so it is removed; the 26 boundary voxels each
    // touch a 0 face and are kept. (V has a hole, so nothing would be removed
    // there — a solid cube is needed to actually test removal.)
    eval("Vs = false(5,5,5); Vs(2:4,2:4,2:4) = true;");
    eval("J = bwmorph3(Vs, 'remove');");
    EXPECT_EQ(static_cast<int>(evalScalar("nnz(J)")), 26);
    EXPECT_DOUBLE_EQ(evalScalar("double(J(3,3,3))"), 0.0);  // interior -> removed
    EXPECT_DOUBLE_EQ(evalScalar("double(J(2,3,3))"), 1.0);  // boundary -> kept
}

TEST_F(Bwmorph3Test, TwoDInputTreatedAsSinglePlane)
{
    eval("B = logical([0 1 1 0; 1 1 1 1; 0 1 1 0; 0 0 1 0]);");
    eval("J = bwmorph3(B, 'clean');");
    EXPECT_EQ(static_cast<int>(evalScalar("size(J,1)")), 4);
    EXPECT_EQ(static_cast<int>(evalScalar("size(J,2)")), 4);
    EXPECT_TRUE(eval("J").type() == ValueType::LOGICAL);
    // 2-D majority can never reach 14 of 27 -> all zero.
    EXPECT_EQ(static_cast<int>(evalScalar("nnz(bwmorph3(B,'majority'))")), 0);
}

TEST_F(Bwmorph3Test, UnknownOperationThrows)
{
    bool threw = false;
    try { eval("bwmorph3(V, 'bogus');"); } catch (...) { threw = true; }
    EXPECT_TRUE(threw);
}
