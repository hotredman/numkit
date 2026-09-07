#include <gtest/gtest.h>
#include <numkit/bundle/standard_library.hpp>
#include <numkit/core/engine.hpp>

using namespace numkit;

class StrfunTest : public ::testing::Test {
protected:
    void SetUp() override {
        engine = makeStandardEngine();
    }

    std::unique_ptr<Engine> engine;
};

TEST_F(StrfunTest, StringComparisonAndSearch) {
    Value sc = engine->eval("strcmp('hello', 'hello');");
    EXPECT_TRUE(sc.toBool());

    Value nsc = engine->eval("strcmp('hello', 'world');");
    EXPECT_FALSE(nsc.toBool());

    Value cnt = engine->eval("contains('Numkit Engine', 'kit');");
    EXPECT_TRUE(cnt.toBool());
}

TEST_F(StrfunTest, StringTransformations) {
    Value u = engine->eval("upper('hello');");
    EXPECT_EQ(u.toString(), "HELLO");

    Value l = engine->eval("lower('WORLD');");
    EXPECT_EQ(l.toString(), "world");

    Value rep = engine->eval("strrep('abc123abc', '123', 'XYZ');");
    EXPECT_EQ(rep.toString(), "abcXYZabc");
}

// --- bugs/closed/lang/num2str-complex-array.md (FIXED) ---
// Complex ARRAY formatting (even all-zero imaginary parts — complex-typed
// storage of real data) must format like MATLAB, not throw.
TEST_F(StrfunTest, Num2strComplexArray)
{
    // MATLAB prints imag==0 elements as plain real numbers (the probe's
    // '13    24' was fprintf row-concatenation; the actual char matrix is
    // '1  2' / '3  4'). Must not throw, must be char, must not emit 'i',
    // and a genuinely-complex element keeps the a±bi form.
    Value s = engine->eval("num2str(complex([1 2; 3 4]));");
    EXPECT_EQ(s.toString().find('i'), std::string::npos);
    EXPECT_EQ(engine->eval("class(num2str(complex([1 2; 3 4])));").toString(), "char");
    Value m = engine->eval("num2str(complex([1, 2+3i]));");
    EXPECT_NE(m.toString().find("+3i"), std::string::npos);
}

// --- inline (legacy constructor, fieldtest portion 10) ---
TEST_F(StrfunTest, InlineConstructor)
{
    Value g = engine->eval("g = inline('a+b','a','b');");
    Value r = engine->eval("g(2,3);");
    EXPECT_NEAR(r.toScalar(), 5.0, 1e-12);
    EXPECT_NEAR(engine->eval("f2 = inline('x^2','x'); f2(7);").toScalar(), 49.0, 1e-12);
    EXPECT_NEAR(engine->eval("f3 = inline('5*sin(2*pi*1*t).*exp(-.4*t)','t'); f3(0.5);").toScalar(),
                5.01328e-16, 1e-20);
}

// --- inline object semantics (MATLAB-probed 2026-09-05) ---
TEST_F(StrfunTest, InlineObjectSemantics)
{
    Value g = engine->eval("g = inline('a+b','a','b');");
    EXPECT_EQ(engine->eval("class(g);").toString(), "inline");
    EXPECT_EQ(engine->eval("formula(g);").toString(), "a+b");
    EXPECT_EQ(engine->eval("char(g);").toString(), "a+b");
    EXPECT_NEAR(engine->eval("g(2,3);").toScalar(), 5.0, 1e-12);
}

// --- num2str complex-array exact spacing (MATLAB-probed 2026-09-05) ---
TEST_F(StrfunTest, Num2strComplexArraySpacing)
{
    EXPECT_EQ(engine->eval("num2str(complex([1.5 2.25 3.125]));").toString(),
              "1.5        2.25       3.125");
    // toString() on a char MATRIX concatenates columns — compare per row.
    EXPECT_EQ(engine->eval("b = num2str(complex([1 2; 3 4])); b(1,:);").toString(), "1  2");
    EXPECT_EQ(engine->eval("b = num2str(complex([1 2; 3 4])); b(2,:);").toString(), "3  4");
    EXPECT_EQ(engine->eval("num2str(complex([1+2i 3-4i]));").toString(), "1+2i   3-4i");
    EXPECT_EQ(engine->eval("size(num2str(complex([1 2; 3 4])), 1);").toScalar(), 2.0);
}

// --- bugs/closed/lang/handle-call-csl-splat.md (FIXED; dual-engine live
// guard) — f(c{:}) with a VARIABLE callee flattens the CSL into the call
// args on BOTH backends (CALL_INDIRECT_FLATTEN on the VM; the TW always
// spliced). Values MATLAB-probed R2025b. Also covers the retired inline
// feval-bridge: inline objects now call their stored handle directly.
TEST_F(StrfunTest, InlineObjectCallAfterBridgeRetirement)
{
    engine->eval("f = inline('2*t', 't');");
    EXPECT_EQ(engine->eval("class(f);").toString(), "inline");
    EXPECT_NEAR(engine->eval("f(3);").toScalar(), 6.0, 1e-12);  // direct obj.fh(subs{:})
    engine->eval("g = inline('a + b', 'a', 'b');");
    EXPECT_NEAR(engine->eval("g(2, 5);").toScalar(), 7.0, 1e-12);
}
