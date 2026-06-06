// libs/image/tests/chromadapt_test.cpp
//
// Regression guard for chromadapt — Bradford / von Kries / Simple
// chromatic adaptation. References from MATLAB R2025b probe.

#include <numkit/core/engine.hpp>
#include <gtest/gtest.h>

using namespace numkit;

class ChromaAdaptTest : public ::testing::Test
{
public:
    StdEngine engine;
    void SetUp() override
    {
        engine.eval(
            "import compat.*;"
            "A = uint8(reshape(linspace(20, 240, 48), [4 4 3]));"
            "ill = [220 200 160];");
    }
    Value eval(const std::string &c) { return engine.eval(c); }
    double evalScalar(const std::string &c) { return eval(c).toScalar(); }
};

TEST_F(ChromaAdaptTest, DefaultBradfordOutputShape)
{
    eval("B = chromadapt(A, ill);");
    EXPECT_EQ(eval("class(B)").toString(), "uint8");
    EXPECT_EQ(static_cast<int>(evalScalar("size(B, 3)")), 3);
}

TEST_F(ChromaAdaptTest, BradfordBitExact)
{
    eval("B = chromadapt(A, ill);");
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(2,2,1))")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(2,2,2))")), 117);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(2,2,3))")), 241);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(3,3,1))")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(3,3,2))")), 142);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(3,3,3))")), 255);
}

TEST_F(ChromaAdaptTest, VonKriesBitExact)
{
    eval("B = chromadapt(A, ill, 'Method', 'vonkries');");
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(2,2,1))")), 46);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(2,2,2))")), 107);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(2,2,3))")), 240);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(3,3,1))")), 64);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(3,3,2))")), 131);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(3,3,3))")), 255);
}

TEST_F(ChromaAdaptTest, SimpleBitExact)
{
    eval("B = chromadapt(A, ill, 'Method', 'simple');");
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(2,2,1))")), 39);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(2,2,2))")), 119);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(2,2,3))")), 247);
}

TEST_F(ChromaAdaptTest, LinearRGBBitExact)
{
    eval("B = chromadapt(A, ill, 'ColorSpace', 'linear-rgb');");
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(2,2,1))")), 34);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(2,2,2))")), 118);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(2,2,3))")), 239);
}

TEST_F(ChromaAdaptTest, AdobeRGBBitExact)
{
    eval("B = chromadapt(A, ill, 'ColorSpace', 'adobe-rgb-1998');");
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(2,2,1))")), 0);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(2,2,2))")), 118);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(2,2,3))")), 241);
}

TEST_F(ChromaAdaptTest, ProPhotoRGBBitExact)
{
    eval("B = chromadapt(A, ill, 'ColorSpace', 'prophoto-rgb');");
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(2,2,1))")), 60);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(2,2,2))")), 123);
    EXPECT_EQ(static_cast<int>(evalScalar("double(B(2,2,3))")), 248);
}

TEST_F(ChromaAdaptTest, WhiteIlluminantIsIdentity)
{
    eval("B = chromadapt(A, [255 255 255]);"
         "diff_v = double(B) - double(A);"
         "m = max(abs(diff_v(:)));");
    EXPECT_LE(evalScalar("m"), 1.0);
}

TEST_F(ChromaAdaptTest, DoubleInputPreserved)
{
    eval("Ad = double(A)/255;"
         "B = chromadapt(Ad, ill);");
    EXPECT_EQ(eval("class(B)").toString(), "double");
}

TEST_F(ChromaAdaptTest, SingleInputPreserved)
{
    eval("As = single(A)/255;"
         "B = chromadapt(As, ill);");
    EXPECT_EQ(eval("class(B)").toString(), "single");
}

TEST_F(ChromaAdaptTest, BadShapeThrows)
{
    EXPECT_THROW(eval("chromadapt(uint8(zeros(4,4)), ill);"),
                 std::exception);
}

TEST_F(ChromaAdaptTest, BadIlluminantSizeThrows)
{
    EXPECT_THROW(eval("chromadapt(A, [1 1]);"), std::exception);
}

TEST_F(ChromaAdaptTest, BlackIlluminantThrows)
{
    EXPECT_THROW(eval("chromadapt(A, [0 0 0]);"), std::exception);
}

TEST_F(ChromaAdaptTest, BadMethodThrows)
{
    EXPECT_THROW(eval("chromadapt(A, ill, 'Method', 'cat02');"),
                 std::exception);
}

TEST_F(ChromaAdaptTest, BadColorSpaceThrows)
{
    EXPECT_THROW(eval("chromadapt(A, ill, 'ColorSpace', 'lab');"),
                 std::exception);
}
