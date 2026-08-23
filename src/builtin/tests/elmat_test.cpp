#include <gtest/gtest.h>
#include <numkit/bundle/standard_library.hpp>
#include <numkit/core/engine.hpp>

using namespace numkit;

class ElmatTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = makeStandardEngine();
    }

    std::unique_ptr<Engine> engine;
};

TEST_F(ElmatTest, ArrayCreation) {
    Value z = engine->eval("zeros(3, 4);");
    EXPECT_EQ(z.dims().rows(), 3u);
    EXPECT_EQ(z.dims().cols(), 4u);

    Value o = engine->eval("ones(2, 5);");
    EXPECT_EQ(o.dims().rows(), 2u);
    EXPECT_EQ(o.dims().cols(), 5u);

    Value e = engine->eval("eye(3);");
    EXPECT_EQ(e.dims().rows(), 3u);
    EXPECT_EQ(e.dims().cols(), 3u);

    Value ls = engine->eval("linspace(0, 10, 5);");
    EXPECT_EQ(ls.numel(), 5u);
}

TEST_F(ElmatTest, MatrixManipulation) {
    Value r = engine->eval("A = [1 2; 3 4]; reshape(A, 1, 4);");
    EXPECT_EQ(r.dims().rows(), 1u);
    EXPECT_EQ(r.dims().cols(), 4u);

    Value d = engine->eval("diag([1 2 3]);");
    EXPECT_EQ(d.dims().rows(), 3u);
    EXPECT_EQ(d.dims().cols(), 3u);

    Value rep = engine->eval("repmat([1 2], 2, 3);");
    EXPECT_EQ(rep.dims().rows(), 2u);
    EXPECT_EQ(rep.dims().cols(), 6u);

    Value f = engine->eval("fliplr([1 2 3]);");
    EXPECT_EQ(f.numel(), 3u);
}

TEST_F(ElmatTest, InspectionAndPredicates) {
    Value sz = engine->eval("size([1 2 3; 4 5 6]);");
    EXPECT_EQ(sz.numel(), 2u);

    Value len = engine->eval("length([1 2 3 4 5]);");
    EXPECT_DOUBLE_EQ(len.toScalar(), 5.0);

    Value ism = engine->eval("ismatrix([1 2; 3 4]);");
    EXPECT_TRUE(ism.toBool());

    Value issc = engine->eval("isscalar(42);");
    EXPECT_TRUE(issc.toBool());

    Value isn = engine->eval("isnan(NaN);");
    EXPECT_TRUE(isn.toBool());
}
