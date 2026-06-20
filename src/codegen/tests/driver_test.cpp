// codegen/tests/driver_test.cpp
//
// The `numkit build` driver core (DESIGN.md §8 M4): the CLI-independent
// pieces — the -args type-spec parser and the parse->emit transpile step.
// (Compilation of the emitted TU is covered by the e2e tests.)

#include <numkit/codegen/driver.hpp>

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

using numkit::ValueType;
using namespace numkit::codegen;

TEST(Driver, ParseTypeSpecScalarsAndArrays)
{
    const auto t = driver::parseTypeSpec("double[], double, single");
    ASSERT_EQ(t.size(), 3u);
    EXPECT_EQ(t[0].dtype, ValueType::DOUBLE);
    EXPECT_FALSE(t[0].shape.isScalar());  // [] -> row vector
    EXPECT_EQ(t[1].dtype, ValueType::DOUBLE);
    EXPECT_TRUE(t[1].shape.isScalar());
    EXPECT_EQ(t[2].dtype, ValueType::SINGLE);
    EXPECT_TRUE(t[2].shape.isScalar());
}

TEST(Driver, ParseTypeSpecEmptyIsNullary)
{
    EXPECT_TRUE(driver::parseTypeSpec("").empty());
    EXPECT_TRUE(driver::parseTypeSpec("   ").empty());
}

TEST(Driver, ParseTypeSpecBadTypeThrows)
{
    EXPECT_THROW(driver::parseTypeSpec("notatype"), std::runtime_error);
    EXPECT_THROW(driver::parseTypeSpec("double, "), std::runtime_error);  // empty token
}

TEST(Driver, TranspileSoleScalarFunction)
{
    const EmittedFunction em = driver::transpileSource(
        "function y = f(a, b)\n  y = a + b;\nend\n", "", driver::parseTypeSpec("double, double"));
    EXPECT_EQ(em.signature, "double f__d__d(double a, double b)");  // mangled program entry
    EXPECT_NE(em.source.find("return y;"), std::string::npos);
}

TEST(Driver, TranspileNamedEntryWithArrayArg)
{
    const EmittedFunction em = driver::transpileSource(
        "function y = biquad(x, b0)\n  n = numel(x);\n  y = zeros(1, n);\n"
        "  for k = 1:n\n    y(k) = b0 * x(k);\n  end\nend\n",
        "biquad", driver::parseTypeSpec("double[], double"));
    EXPECT_NE(em.source.find("biquad"), std::string::npos);
    EXPECT_NE(em.signature.find("const double* x, std::size_t x_len"), std::string::npos);
}

TEST(Driver, ArityMismatchThrows)
{
    EXPECT_THROW(driver::transpileSource("function y = f(a)\n  y = a;\nend\n", "",
                                         driver::parseTypeSpec("double, double")),
                 std::runtime_error);
}

TEST(Driver, MultiFunctionRequiresExplicitEntry)
{
    const char *src = "function y = f(a)\n  y = a;\nend\n"
                      "function z = g(a)\n  z = a;\nend\n";
    EXPECT_THROW(driver::transpileSource(src, "", driver::parseTypeSpec("double")),
                 std::runtime_error);
    // ...but naming one works.
    const EmittedFunction em = driver::transpileSource(src, "g", driver::parseTypeSpec("double"));
    EXPECT_NE(em.source.find("g__d"), std::string::npos);
}

TEST(Driver, UnknownEntryThrows)
{
    EXPECT_THROW(driver::transpileSource("function y = f(a)\n  y = a;\nend\n", "nope",
                                         driver::parseTypeSpec("double")),
                 std::runtime_error);
}

TEST(Driver, BridgeOptionEmitsBridgedCall)
{
    BridgeOptions b;
    b.enabled       = true;
    b.runtimeHeader = "nk_codegen_rt.h";
    const EmittedFunction em = driver::transpileSource(
        "function y = f(x)\n  y = sign(x);\nend\n", "", driver::parseTypeSpec("double"), b);
    EXPECT_NE(em.source.find("nk_rt::bridge_scalar(\"sign\""), std::string::npos);
}

TEST(Driver, TranspileToPluginEmitsRegisterHook)
{
    const std::string tu = driver::transpileToPlugin(
        "function s = mysum(v)\n  n = numel(v);\n  s = 0;\n"
        "  for k = 1:n\n    s = s + v(k);\n  end\nend\n",
        "", driver::parseTypeSpec("double[]"), "my_sum", "nk_plugin.h");
    EXPECT_NE(tu.find("#include \"nk_plugin.h\""), std::string::npos);
    EXPECT_NE(tu.find("nk_plugin_register"), std::string::npos);
    EXPECT_NE(tu.find("register_fn(\"my_sum\""), std::string::npos);
}

TEST(Driver, TranspileToPluginRefusesArrayOutput)
{
    // array OUTPUT isn't tierable yet -> emitScalarPlugin refuses.
    EXPECT_THROW(driver::transpileToPlugin("function y = f(v)\n  y = v * 2;\nend\n", "",
                                           driver::parseTypeSpec("double[]"), "f", "nk_plugin.h"),
                 std::runtime_error);
}
