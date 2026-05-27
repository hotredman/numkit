// libs/image/tests/modefilt_test.cpp
//
// Regression guard for image/modefilt. Fingerprints from MATLAB R2025b.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

class ModefiltTest : public ::testing::Test
{
public:
    numkit::Engine engine;
    void SetUp() override
    {
        engine.eval("import compat.*;");
        engine.eval("A = uint8([1 1 2 2; 1 3 2 4; 5 5 6 6; 5 7 6 8]);");
    }
    double evalScalar(const std::string &c) { return engine.eval(c).toScalar(); }
};

// Default 3x3 symmetric: clean expected from MATLAB.
TEST_F(ModefiltTest, DefaultSymmetric)
{
    engine.eval("B = modefilt(A);");
    EXPECT_DOUBLE_EQ(evalScalar("size(B,1)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("size(B,2)"), 4.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(1,4)"), 2.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(2,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(2,2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(3,3)"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(4,4)"), 6.0);
}

TEST_F(ModefiltTest, Replicate)
{
    engine.eval("B = modefilt(A, [3 3], 'replicate');");
    EXPECT_DOUBLE_EQ(evalScalar("B(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(2,2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(3,3)"), 6.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(4,4)"), 6.0);
}

TEST_F(ModefiltTest, FiveByFiveSymmetric)
{
    engine.eval("B = modefilt(A, [5 5]);");
    EXPECT_DOUBLE_EQ(evalScalar("B(1,1)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(2,2)"), 1.0);
    EXPECT_DOUBLE_EQ(evalScalar("B(4,4)"), 6.0);
}

// Clear non-tie case: 5x5 of ones with one outlier → mode = 1.
TEST_F(ModefiltTest, MajorityWins)
{
    engine.eval("A2 = uint8(ones(5)); A2(3,3) = 7; "
                "B = modefilt(A2, [3 3], 'symmetric');");
    EXPECT_DOUBLE_EQ(evalScalar("B(3,3)"), 1.0);  // 8 ones beat 1 seven
}

// Smallest-on-tie rule.
TEST_F(ModefiltTest, SmallestOnTie)
{
    engine.eval("A2 = uint8([0 1 0; 1 0 1; 0 1 0]); "
                "B = modefilt(A2, [3 3], 'symmetric');");
    // Symmetric 3x3 at center: 5 zeros + 4 ones → 0 wins clearly.
    EXPECT_DOUBLE_EQ(evalScalar("B(2,2)"), 0.0);
}

// Output class preservation.
TEST_F(ModefiltTest, PreservesUint16)
{
    engine.eval("A2 = uint16([1000 2000; 1000 3000]); "
                "B = modefilt(A2, [3 3], 'symmetric');");
    EXPECT_EQ(engine.eval("class(B)").toString(), "uint16");
}

// Errors: even filter size.
TEST_F(ModefiltTest, EvenFilterSizeThrows)
{
    EXPECT_THROW(engine.eval("modefilt(A, [4 3]);"), std::exception);
    EXPECT_THROW(engine.eval("modefilt(A, [3 4]);"), std::exception);
}

// Unknown padopt throws.
TEST_F(ModefiltTest, UnknownPadOptThrows)
{
    EXPECT_THROW(engine.eval("modefilt(A, [3 3], 'bogus');"), std::exception);
}
