#include <gtest/gtest.h>
#include <numkit/builtin/lang.hpp>
#include <numkit/bundle/standard_library.hpp>
#include <numkit/core/engine.hpp>

using namespace numkit;

class LangTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = makeStandardEngine();
    }

    std::unique_ptr<Engine> engine;
};

TEST_F(LangTest, Eval) {
    Value e = engine->eval("eval('2 + 3');");
    EXPECT_DOUBLE_EQ(e.toScalar(), 5.0);
}

TEST_F(LangTest, KeywordsAndVarNames) {
    Value kw = engine->eval("iskeyword('for');");
    EXPECT_TRUE(kw.toBool());

    Value nkw = engine->eval("iskeyword('my_var');");
    EXPECT_FALSE(nkw.toBool());

    Value vn = engine->eval("isvarname('valid_123');");
    EXPECT_TRUE(vn.toBool());

    Value nvn = engine->eval("isvarname('123_invalid');");
    EXPECT_FALSE(nvn.toBool());
}

TEST_F(LangTest, DirectCppLangAPI) {
    // Keywords
    EXPECT_TRUE(builtin::iskeyword("for"));
    EXPECT_TRUE(builtin::iskeyword("while"));
    EXPECT_TRUE(builtin::iskeyword("function"));
    EXPECT_FALSE(builtin::iskeyword("my_custom_var"));

    const auto &kwList = builtin::keywords();
    EXPECT_GT(kwList.size(), 15u);
    EXPECT_NE(std::find(kwList.begin(), kwList.end(), "for"), kwList.end());

    // iskeyword Span overload returning cell array
    Value allKw = builtin::iskeyword(Span<const Value>{});
    EXPECT_TRUE(allKw.isCell());
    EXPECT_EQ(allKw.numel(), kwList.size());

    // Variable names
    EXPECT_TRUE(builtin::isvarname("valid_var_123"));
    EXPECT_TRUE(builtin::isvarname("A"));
    EXPECT_FALSE(builtin::isvarname("123_invalid"));
    EXPECT_FALSE(builtin::isvarname(""));
    EXPECT_FALSE(builtin::isvarname("invalid-dash"));
    EXPECT_FALSE(builtin::isvarname("for")); // keyword is not a valid var name

    // isvarname Value overload
    EXPECT_TRUE(builtin::isvarname(Value::fromString("validName")).toBool());
    EXPECT_FALSE(builtin::isvarname(Value::scalar(123.0)).toBool());

    // Environment variables
    builtin::setenv("NUMKIT_CPP_API_TEST", "active_42");
    EXPECT_EQ(builtin::getenv("NUMKIT_CPP_API_TEST"), "active_42");

    Value envVal = builtin::getenv(Value::fromString("NUMKIT_CPP_API_TEST"));
    EXPECT_EQ(envVal.toString(), "active_42");

    builtin::setenv("NUMKIT_CPP_API_TEST", "");
    EXPECT_EQ(builtin::getenv("NUMKIT_CPP_API_TEST"), "");
}
