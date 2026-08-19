#include <gtest/gtest.h>
#include <numkit/bundle/standard_library.hpp>
#include <numkit/core/engine.hpp>

using namespace numkit;

class HelpSystemTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = makeStandardEngine();
        engine->eval("import compat.*;");
    }

    std::unique_ptr<Engine> engine;
};

TEST_F(HelpSystemTest, HelpAllCategoriesReturnString) {
    Value res = engine->eval("h = help();");
    EXPECT_TRUE(res.isChar() || res.isString());
    std::string text = res.toString();
    EXPECT_NE(text.find("Numkit Help Topics:"), std::string::npos);
    EXPECT_NE(text.find("elmat"), std::string::npos);
    EXPECT_NE(text.find("elfun"), std::string::npos);
    EXPECT_NE(text.find("matfun"), std::string::npos);
    EXPECT_NE(text.find("image"), std::string::npos);
}

TEST_F(HelpSystemTest, HelpCategoryElmat) {
    Value res = engine->eval("h = help('elmat');");
    std::string text = res.toString();
    EXPECT_NE(text.find("Elementary matrices and matrix manipulation"), std::string::npos);
    EXPECT_NE(text.find("zeros"), std::string::npos);
    EXPECT_NE(text.find("ones"), std::string::npos);
    EXPECT_NE(text.find("eye"), std::string::npos);
    EXPECT_NE(text.find("diag"), std::string::npos);
    EXPECT_NE(text.find("reshape"), std::string::npos);
}

TEST_F(HelpSystemTest, HelpCategoryElfun) {
    Value res = engine->eval("h = help('elfun');");
    std::string text = res.toString();
    EXPECT_NE(text.find("Elementary math functions"), std::string::npos);
    EXPECT_NE(text.find("sin"), std::string::npos);
    EXPECT_NE(text.find("cos"), std::string::npos);
    EXPECT_NE(text.find("exp"), std::string::npos);
    EXPECT_NE(text.find("log"), std::string::npos);
}

TEST_F(HelpSystemTest, HelpFunctionDoc) {
    Value res = engine->eval("h = help('sin');");
    std::string text = res.toString();
    EXPECT_NE(text.find("SIN"), std::string::npos);
    EXPECT_NE(text.find("Sine"), std::string::npos);

    Value resSvd = engine->eval("h = help('svd');");
    std::string textSvd = resSvd.toString();
    EXPECT_NE(textSvd.find("SVD"), std::string::npos);
    EXPECT_NE(textSvd.find("Singular value decomposition"), std::string::npos);
}

TEST_F(HelpSystemTest, WhatCategory) {
    Value res = engine->eval("w = what('elmat');");
    EXPECT_TRUE(res.isStruct());
    Value mField = engine->eval("w.m;");
    EXPECT_TRUE(mField.isCell());
    EXPECT_GT(mField.numel(), 10u);
}

TEST_F(HelpSystemTest, BuiltinsQuery) {
    Value all = engine->eval("b = builtins();");
    EXPECT_TRUE(all.isCell());
    EXPECT_GT(all.numel(), 100u);

    Value elmatOnly = engine->eval("b_elmat = builtins('elmat');");
    EXPECT_TRUE(elmatOnly.isCell());
    EXPECT_GT(elmatOnly.numel(), 10u);
    EXPECT_LT(elmatOnly.numel(), all.numel());
}

TEST_F(HelpSystemTest, InmemQuery) {
    Value m = engine->eval("[m, mex, c] = inmem();");
    EXPECT_TRUE(m.isCell());
}
