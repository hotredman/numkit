#include <gtest/gtest.h>
#include <numkit/builtin/general.hpp>
#include <numkit/bundle/standard_library.hpp>
#include <numkit/core/engine.hpp>

using namespace numkit;

class GeneralTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = makeStandardEngine();
    }

    std::unique_ptr<Engine> engine;
};

TEST_F(GeneralTest, WorkspaceVariablesAndExist) {
    engine->eval("my_var = 123;");
    Value ex = engine->eval("exist('my_var', 'var');");
    EXPECT_DOUBLE_EQ(ex.toScalar(), 1.0);

    engine->eval("clear my_var;");
    Value ex2 = engine->eval("exist('my_var', 'var');");
    EXPECT_DOUBLE_EQ(ex2.toScalar(), 0.0);
}

TEST_F(GeneralTest, HelpAndBuiltins) {
    Value h = engine->eval("h = help('elmat');");
    EXPECT_TRUE(h.isChar() || h.isString());
    EXPECT_NE(h.toString().find("zeros"), std::string::npos);

    Value b = engine->eval("builtins('elmat');");
    EXPECT_TRUE(b.isCell());
    EXPECT_GT(b.numel(), 10u);
}

TEST_F(GeneralTest, DirectCppGeneralAPI) {
    // Categories
    std::vector<std::string> cats = builtin::categories();
    EXPECT_GT(cats.size(), 8u);
    EXPECT_NE(std::find(cats.begin(), cats.end(), "elmat"), cats.end());
    EXPECT_NE(std::find(cats.begin(), cats.end(), "elfun"), cats.end());

    // Help query
    std::string hAll = builtin::help();
    EXPECT_FALSE(hAll.empty());
    EXPECT_NE(hAll.find("elmat"), std::string::npos);

    std::string hCat = builtin::help("elmat");
    EXPECT_NE(hCat.find("zeros"), std::string::npos);

    std::string hFunc = builtin::help("sin");
    EXPECT_NE(hFunc.find("sin"), std::string::npos);

    // What
    std::vector<std::string> elmatFuncs = builtin::what("elmat");
    EXPECT_GT(elmatFuncs.size(), 10u);
    EXPECT_NE(std::find(elmatFuncs.begin(), elmatFuncs.end(), "zeros"), elmatFuncs.end());
    EXPECT_NE(std::find(elmatFuncs.begin(), elmatFuncs.end(), "ones"), elmatFuncs.end());

    // Builtins
    std::vector<std::string> elfunBuiltins = builtin::builtins("elfun");
    EXPECT_GT(elfunBuiltins.size(), 10u);
    EXPECT_NE(std::find(elfunBuiltins.begin(), elfunBuiltins.end(), "sin"), elfunBuiltins.end());

    // Value overloads
    Value vWhat = builtin::what(Span<const Value>{});
    EXPECT_TRUE(vWhat.isStruct());
    EXPECT_TRUE(vWhat.hasField("m"));
    EXPECT_TRUE(vWhat.field("m").isCell());
    EXPECT_GT(vWhat.field("m").numel(), 10u);
}
