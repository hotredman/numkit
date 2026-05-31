// libs/image/tests/labelmatrix_test.cpp
//
// Regression guard for labelmatrix + cc2bw — CC struct conversions.
// Reference values from MATLAB R2025b probe. Also exercises the
// column-major component-numbering convention that bwconncomp must
// follow for parity (this cycle's fix to label_components).

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class CCStructTest : public ::testing::Test
{
public:
    Engine engine;
    void SetUp() override
    {
        engine.eval(
            "import compat.*;"
            "BW = logical([1 1 0 0 0;"
            "              1 1 0 1 1;"
            "              0 0 0 1 0;"
            "              0 1 1 0 0;"
            "              0 1 0 0 1]);"
            "CC = bwconncomp(BW, 4);");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

// ── labelmatrix: column-major label assignment ─────────────────

TEST_F(CCStructTest, LabelMatrixOutputClassUint8)
{
    eval("L = labelmatrix(CC);");
    EXPECT_EQ(eval("class(L)").toString(), "uint8");
    EXPECT_EQ(static_cast<int>(evalScalar("size(L, 1)")), 5);
    EXPECT_EQ(static_cast<int>(evalScalar("size(L, 2)")), 5);
}

TEST_F(CCStructTest, LabelMatrixValues)
{
    eval("L = labelmatrix(CC);");
    // Column-major order:
    //   label 1 = top-left 2x2 block (cols 0-1, rows 0-1)
    //   label 2 = bottom-mid (cols 1-2, rows 3-4)
    //   label 3 = middle-right (cols 3-4, rows 1-2)
    //   label 4 = bottom-right (col 4, row 4)
    EXPECT_EQ(static_cast<int>(evalScalar("double(L(1,1))")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("double(L(2,2))")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("double(L(4,2))")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("double(L(5,2))")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("double(L(4,3))")), 2);
    EXPECT_EQ(static_cast<int>(evalScalar("double(L(2,4))")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("double(L(3,4))")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("double(L(2,5))")), 3);
    EXPECT_EQ(static_cast<int>(evalScalar("double(L(5,5))")), 4);
    // Background = 0.
    EXPECT_EQ(static_cast<int>(evalScalar("double(L(1,3))")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("double(L(3,3))")), 0);
}

TEST_F(CCStructTest, LabelMatrixEmptyCCIsAllZero)
{
    eval("BW0 = false(4, 4);"
         "CC0 = bwconncomp(BW0);"
         "L0 = labelmatrix(CC0);");
    EXPECT_EQ(eval("class(L0)").toString(), "uint8");
    EXPECT_EQ(static_cast<int>(evalScalar("sum(L0(:))")), 0);
}

// ── cc2bw default = original BW ────────────────────────────────

TEST_F(CCStructTest, CC2BWDefaultEqualsBW)
{
    eval("BW2 = cc2bw(CC);");
    EXPECT_EQ(eval("class(BW2)").toString(), "logical");
    EXPECT_EQ(static_cast<int>(evalScalar("double(isequal(BW2, BW))")), 1);
}

// ── cc2bw ObjectsToKeep ─────────────────────────────────────────

TEST_F(CCStructTest, CC2BWSingleObjectToKeep)
{
    // Keep component 2 (bottom-mid). Expected pixels: (4,2),(5,2),(4,3).
    eval("B = cc2bw(CC, 'ObjectsToKeep', 2);");
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(4,2))")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(5,2))")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(4,3))")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,1))")), 0);  // not kept
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(2,4))")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(5,5))")), 0);
}

TEST_F(CCStructTest, CC2BWVectorObjectsToKeep)
{
    eval("B = cc2bw(CC, 'ObjectsToKeep', [1 3]);");
    // Component 1 (top-left 2x2) + component 3 (middle-right).
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,1))")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(2,2))")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(2,4))")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(3,4))")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(4,2))")), 0);  // not kept
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(5,5))")), 0);
}

TEST_F(CCStructTest, CC2BWLogicalObjectsToKeep)
{
    eval("lv = false(1, CC.NumObjects); lv(1) = true; lv(end) = true;"
         "B = cc2bw(CC, 'ObjectsToKeep', lv);");
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(1,1))")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(5,5))")), 1);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(4,2))")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(2,4))")), 0);
}

// ── Errors ─────────────────────────────────────────────────────

TEST_F(CCStructTest, LabelMatrixNonStructThrows)
{
    EXPECT_THROW(eval("labelmatrix(uint8(1));"), std::exception);
}

TEST_F(CCStructTest, CC2BWBadIndexThrows)
{
    EXPECT_THROW(eval("cc2bw(CC, 'ObjectsToKeep', 99);"), std::exception);
    EXPECT_THROW(eval("cc2bw(CC, 'ObjectsToKeep', -1);"), std::exception);
}

TEST_F(CCStructTest, CC2BWLogicalLengthMismatchThrows)
{
    EXPECT_THROW(eval("cc2bw(CC, 'ObjectsToKeep', logical([1 0]));"),
                 std::exception);
}

TEST_F(CCStructTest, CC2BWUnknownOptionThrows)
{
    EXPECT_THROW(eval("cc2bw(CC, 'Banana', 1);"), std::exception);
}
