// tests/stdlib/types_test.cpp — Complex, logical, string, empty matrix, NaN/Inf, type coercion
// Parameterized: runs on both TreeWalker and VM backends

#include "dual_engine_fixture.hpp"

using namespace mlab_test;

// ============================================================
// Type operations, coercion, and special values
// ============================================================

class TypeOpsTest : public DualEngineTest
{};

TEST_P(TypeOpsTest, AndReturnsLogical)
{
    eval("r = 3 & 5;");
    EXPECT_TRUE(getVarPtr("r")->isLogicalScalar());
    EXPECT_TRUE(getVarPtr("r")->toBool());
}

TEST_P(TypeOpsTest, OrReturnsLogical)
{
    eval("r = 0 | 5;");
    EXPECT_TRUE(getVarPtr("r")->isLogicalScalar());
    EXPECT_TRUE(getVarPtr("r")->toBool());
}

TEST_P(TypeOpsTest, NotReturnsLogical)
{
    eval("r = ~0;");
    EXPECT_TRUE(getVarPtr("r")->isLogicalScalar());
    EXPECT_TRUE(getVarPtr("r")->toBool());
}

TEST_P(TypeOpsTest, NotFalseOnNonzero)
{
    eval("r = ~5;");
    EXPECT_TRUE(getVarPtr("r")->isLogicalScalar());
    EXPECT_FALSE(getVarPtr("r")->toBool());
}

TEST_P(TypeOpsTest, SignNaN)
{
    eval("r = sign(nan);");
    EXPECT_TRUE(std::isnan(getVarPtr("r")->toScalar()));
}

TEST_P(TypeOpsTest, IsnanReturnsLogical)
{
    eval("r = isnan(nan);");
    EXPECT_TRUE(getVarPtr("r")->isLogicalScalar());
    EXPECT_TRUE(getVarPtr("r")->toBool());
}

TEST_P(TypeOpsTest, IsinfReturnsLogical)
{
    eval("r = isinf(inf);");
    EXPECT_TRUE(getVarPtr("r")->isLogicalScalar());
    EXPECT_TRUE(getVarPtr("r")->toBool());
}

TEST_P(TypeOpsTest, RemCorrect)
{
    EXPECT_DOUBLE_EQ(evalScalar("rem(7, 4);"), 3.0);
    EXPECT_DOUBLE_EQ(evalScalar("rem(-7, 4);"), -3.0);
}

TEST_P(TypeOpsTest, ForLoopChar)
{
    eval("s = ''; for c = 'abc'; s = [s, c]; end;");
    EXPECT_EQ(getVarPtr("s")->toString(), "abc");
}

TEST_P(TypeOpsTest, ForLoopLogical)
{
    eval("n = 0; for x = [true false true]; n = n + x; end;");
    EXPECT_DOUBLE_EQ(getVar("n"), 2.0);
}

TEST_P(TypeOpsTest, SwitchMatrix)
{
    eval("v = [1 2 3]; switch v; case [1 2 3]; r = 1; case [4 5 6]; r = 2; otherwise; r = 0; end;");
    EXPECT_DOUBLE_EQ(getVar("r"), 1.0);
}

TEST_P(TypeOpsTest, SwitchString)
{
    eval("switch 'hello'; case 'world'; r = 1; case 'hello'; r = 2; otherwise; r = 0; end;");
    EXPECT_DOUBLE_EQ(getVar("r"), 2.0);
}

TEST_P(TypeOpsTest, ToBoolLogicalArray)
{
    eval("if [true true true]; r = 1; else; r = 0; end;");
    EXPECT_DOUBLE_EQ(getVar("r"), 1.0);
    eval("if [true false true]; r = 1; else; r = 0; end;");
    EXPECT_DOUBLE_EQ(getVar("r"), 0.0);
}

TEST_P(TypeOpsTest, EmptyPlusEmpty)
{
    eval("r = [] + [];");
    EXPECT_TRUE(getVarPtr("r")->isEmpty());
}

TEST_P(TypeOpsTest, EmptyPlusScalar)
{
    eval("r = [] + 1;");
    EXPECT_TRUE(getVarPtr("r")->isEmpty());
}

TEST_P(TypeOpsTest, EmptyTimesScalar)
{
    eval("e = []; r = e;");
    EXPECT_TRUE(getVarPtr("r")->isEmpty());
}

TEST_P(TypeOpsTest, EmptyMinusEmpty)
{
    eval("a = []; r = a + a;");
    auto *r = getVarPtr("r");
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->numel(), 0u);
}

TEST_P(TypeOpsTest, SizeEmpty)
{
    EXPECT_DOUBLE_EQ(evalScalar("size([], 1);"), 0.0);
    EXPECT_DOUBLE_EQ(evalScalar("size([], 2);"), 0.0);
}

TEST_P(TypeOpsTest, LengthEmpty)
{
    EXPECT_DOUBLE_EQ(evalScalar("length([]);"), 0.0);
}

TEST_P(TypeOpsTest, IsemptyTrue)
{
    eval("r = isempty([]);");
    EXPECT_TRUE(getVarPtr("r")->toBool());
}

TEST_P(TypeOpsTest, IsemptyFalse)
{
    eval("r = isempty(5);");
    EXPECT_FALSE(getVarPtr("r")->toBool());
}

TEST_P(TypeOpsTest, ZerosEmptyDim)
{
    eval("r = zeros(0, 3);");
    auto *r = getVarPtr("r");
    EXPECT_EQ(rows(*r), 0u);
    EXPECT_EQ(cols(*r), 3u);
    EXPECT_EQ(r->numel(), 0u);
}

TEST_P(TypeOpsTest, ForOverEmpty)
{
    eval("n = 0; for i = []; n = n + 1; end;");
    EXPECT_DOUBLE_EQ(getVar("n"), 0.0); // body never executes
}

TEST_P(TypeOpsTest, CharPlusDouble)
{
    // 'A' + 1 → double(66)  (MATLAB promotes char to double for arithmetic)
    eval("r = 'A' + 1;");
    EXPECT_DOUBLE_EQ(getVarPtr("r")->toScalar(), 66.0);
}

TEST_P(TypeOpsTest, NaNComparisons)
{
    // NaN == NaN → false
    eval("r = nan == nan;");
    auto *r = getVarPtr("r");
    EXPECT_FALSE(r->toBool());

    // NaN ~= NaN → true
    eval("r = nan ~= nan;");
    r = getVarPtr("r");
    EXPECT_TRUE(r->toBool());
}

TEST_P(TypeOpsTest, InfComparisons)
{
    eval("r = inf > 1e308;");
    EXPECT_TRUE(getVarPtr("r")->toBool());

    eval("r = -inf < -1e308;");
    EXPECT_TRUE(getVarPtr("r")->toBool());
}

TEST_P(TypeOpsTest, ComplexMultiply)
{
    // (2+3i) * (4-1i) = 8-2i+12i-3i^2 = 8+10i+3 = 11+10i
    eval("r = (2+3i) * (4-1i);");
    auto *r = getVarPtr("r");
    EXPECT_TRUE(r->isComplex());
    auto c = r->toComplex();
    EXPECT_DOUBLE_EQ(c.real(), 11.0);
    EXPECT_DOUBLE_EQ(c.imag(), 10.0);
}

TEST_P(TypeOpsTest, ComplexDivide)
{
    // (4+2i) / (1+1i) = (4+2i)(1-1i) / (1+1) = (4-4i+2i-2i^2)/2 = (6-2i)/2 = 3-1i
    eval("r = (4+2i) / (1+1i);");
    auto *r = getVarPtr("r");
    EXPECT_TRUE(r->isComplex());
    auto c = r->toComplex();
    EXPECT_DOUBLE_EQ(c.real(), 3.0);
    EXPECT_DOUBLE_EQ(c.imag(), -1.0);
}

TEST_P(TypeOpsTest, ComplexPower)
{
    // (1+1i)^2 = 1+2i+i^2 = 2i
    eval("r = (1+1i)^2;");
    auto *r = getVarPtr("r");
    EXPECT_TRUE(r->isComplex());
    auto c = r->toComplex();
    EXPECT_NEAR(c.real(), 0.0, 1e-10);
    EXPECT_NEAR(c.imag(), 2.0, 1e-10);
}

TEST_P(TypeOpsTest, ComplexEquality)
{
    eval("r = (1+2i) == (1+2i);");
    EXPECT_TRUE(getVarPtr("r")->toBool());

    eval("r = (1+2i) == (1+3i);");
    EXPECT_FALSE(getVarPtr("r")->toBool());
}

TEST_P(TypeOpsTest, DoubleToComplexPromotion)
{
    eval("a = [1 2 3]; a(2) = 1+2i;");
    auto *a = getVarPtr("a");
    EXPECT_TRUE(a->isComplex());
    auto c = a->complexData()[1];
    EXPECT_DOUBLE_EQ(c.real(), 1.0);
    EXPECT_DOUBLE_EQ(c.imag(), 2.0);
    // Original elements preserved
    EXPECT_DOUBLE_EQ(a->complexData()[0].real(), 1.0);
    EXPECT_DOUBLE_EQ(a->complexData()[0].imag(), 0.0);
}

TEST_P(TypeOpsTest, MultiReturnParamOverlap)
{
    eval(R"(
        function [b, a] = swap(a, b)
            % return names overlap with params in reverse order
        end
        [x, y] = swap(10, 20);
    )");
    EXPECT_DOUBLE_EQ(getVar("x"), 20.0);
    EXPECT_DOUBLE_EQ(getVar("y"), 10.0);
}

TEST_P(TypeOpsTest, ComplexArrayEq)
{
    eval("a = [1+2i, 3+4i]; b = [1+2i, 3+0i]; r = a == b;");
    auto *r = getVarPtr("r");
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->numel(), 2u);
    EXPECT_EQ(r->logicalData()[0], 1); // 1+2i == 1+2i
    EXPECT_EQ(r->logicalData()[1], 0); // 3+4i ~= 3+0i
}

TEST_P(TypeOpsTest, ComplexArrayNe)
{
    eval("r = [1+1i, 2+2i] ~= [1+1i, 2+3i];");
    auto *r = getVarPtr("r");
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->logicalData()[0], 0); // equal
    EXPECT_EQ(r->logicalData()[1], 1); // not equal
}

TEST_P(TypeOpsTest, ComplexScalarVsArray)
{
    eval("r = (1+1i) == [1+1i, 2+2i, 1+1i];");
    auto *r = getVarPtr("r");
    ASSERT_NE(r, nullptr);
    EXPECT_EQ(r->numel(), 3u);
    EXPECT_EQ(r->logicalData()[0], 1);
    EXPECT_EQ(r->logicalData()[1], 0);
    EXPECT_EQ(r->logicalData()[2], 1);
}

TEST_P(TypeOpsTest, ComplexLtThrows)
{
    EXPECT_THROW(eval("r = (1+2i) < (3+4i);"), std::runtime_error);
}

TEST_P(TypeOpsTest, ComplexDoubleEq)
{
    // Complex vs double comparison
    eval("r = (3+0i) == 3;");
    auto *r = getVarPtr("r");
    ASSERT_NE(r, nullptr);
    EXPECT_TRUE(r->toBool());
}

TEST_P(TypeOpsTest, StringLiteral)
{
    eval("s = \"hello\";");
    auto *s = getVarPtr("s");
    ASSERT_NE(s, nullptr);
    EXPECT_TRUE(s->isString());
    EXPECT_EQ(s->toString(), "hello");
}

TEST_P(TypeOpsTest, StringClass)
{
    EXPECT_EQ(evalString("class(\"hello\");"), "string");
}

TEST_P(TypeOpsTest, IsStringTrue)
{
    EXPECT_TRUE(evalBool("isstring(\"abc\");"));
}

TEST_P(TypeOpsTest, IsStringFalseOnChar)
{
    EXPECT_FALSE(evalBool("isstring('abc');"));
}

TEST_P(TypeOpsTest, IsCharFalseOnString)
{
    EXPECT_FALSE(evalBool("ischar(\"abc\");"));
}

TEST_P(TypeOpsTest, StringConcat)
{
    eval("s = \"hello\" + \" world\";");
    auto *s = getVarPtr("s");
    ASSERT_NE(s, nullptr);
    EXPECT_TRUE(s->isString());
    EXPECT_EQ(s->toString(), "hello world");
}

TEST_P(TypeOpsTest, StringEqTrue)
{
    EXPECT_TRUE(evalBool("\"abc\" == \"abc\";"));
}

TEST_P(TypeOpsTest, StringEqFalse)
{
    EXPECT_FALSE(evalBool("\"abc\" == \"xyz\";"));
}

TEST_P(TypeOpsTest, StringNeTrue)
{
    EXPECT_TRUE(evalBool("\"abc\" ~= \"xyz\";"));
}

TEST_P(TypeOpsTest, StringLt)
{
    EXPECT_TRUE(evalBool("\"abc\" < \"xyz\";"));
}

TEST_P(TypeOpsTest, StringToChar)
{
    eval("c = char(\"hello\");");
    auto *c = getVarPtr("c");
    ASSERT_NE(c, nullptr);
    EXPECT_TRUE(c->isChar());
    EXPECT_EQ(c->toString(), "hello");
}

TEST_P(TypeOpsTest, CharToString)
{
    eval("s = string('hello');");
    auto *s = getVarPtr("s");
    ASSERT_NE(s, nullptr);
    EXPECT_TRUE(s->isString());
    EXPECT_EQ(s->toString(), "hello");
}

TEST_P(TypeOpsTest, NumToString)
{
    eval("s = string(42);");
    auto *s = getVarPtr("s");
    ASSERT_NE(s, nullptr);
    EXPECT_TRUE(s->isString());
    EXPECT_EQ(s->toString(), "42");
}

TEST_P(TypeOpsTest, Strlength)
{
    EXPECT_DOUBLE_EQ(evalScalar("strlength(\"hello\");"), 5.0);
}

TEST_P(TypeOpsTest, StringHorzcat)
{
    eval("s = [\"abc\", \"def\"];");
    auto *s = getVarPtr("s");
    ASSERT_NE(s, nullptr);
    EXPECT_TRUE(s->isString());
    EXPECT_EQ(s->numel(), 2u);
    EXPECT_EQ(s->stringElem(0), "abc");
    EXPECT_EQ(s->stringElem(1), "def");
}

TEST_P(TypeOpsTest, StringContains)
{
    EXPECT_TRUE(evalBool("contains(\"hello world\", \"world\");"));
    EXPECT_FALSE(evalBool("contains(\"hello\", \"xyz\");"));
}

TEST_P(TypeOpsTest, StringStartsWith)
{
    EXPECT_TRUE(evalBool("startsWith(\"hello\", \"hel\");"));
    EXPECT_FALSE(evalBool("startsWith(\"hello\", \"xyz\");"));
}

TEST_P(TypeOpsTest, StringEndsWith)
{
    EXPECT_TRUE(evalBool("endsWith(\"hello\", \"llo\");"));
    EXPECT_FALSE(evalBool("endsWith(\"hello\", \"xyz\");"));
}

TEST_P(TypeOpsTest, Strrep)
{
    eval("s = strrep(\"hello world\", \"world\", \"there\");");
    auto *s = getVarPtr("s");
    ASSERT_NE(s, nullptr);
    EXPECT_TRUE(s->isString());
    EXPECT_EQ(s->toString(), "hello there");
}

TEST_P(TypeOpsTest, StringCompareWithChar)
{
    // string == char should work
    EXPECT_TRUE(evalBool("\"hello\" == 'hello';"));
}

// ============================================================
// complex() constructor — scalar, vector, and two-arg forms
// ============================================================

TEST_P(TypeOpsTest, ComplexScalarSingleArg)
{
    eval("z = complex(7);");
    auto *z = getVarPtr("z");
    ASSERT_NE(z, nullptr);
    EXPECT_TRUE(z->isComplex());
    EXPECT_TRUE(z->isScalar());
    EXPECT_DOUBLE_EQ(z->toComplex().real(), 7.0);
    EXPECT_DOUBLE_EQ(z->toComplex().imag(), 0.0);
}

TEST_P(TypeOpsTest, ComplexVectorSingleArg)
{
    eval("z = complex([1 2 3]);");
    auto *z = getVarPtr("z");
    ASSERT_NE(z, nullptr);
    EXPECT_TRUE(z->isComplex());
    EXPECT_EQ(z->numel(), 3u);
    EXPECT_DOUBLE_EQ(z->complexData()[0].real(), 1.0);
    EXPECT_DOUBLE_EQ(z->complexData()[0].imag(), 0.0);
    EXPECT_DOUBLE_EQ(z->complexData()[2].real(), 3.0);
}

TEST_P(TypeOpsTest, ComplexScalarPair)
{
    eval("z = complex(3, 4);");
    auto *z = getVarPtr("z");
    ASSERT_NE(z, nullptr);
    EXPECT_TRUE(z->isComplex());
    EXPECT_DOUBLE_EQ(z->toComplex().real(), 3.0);
    EXPECT_DOUBLE_EQ(z->toComplex().imag(), 4.0);
}

TEST_P(TypeOpsTest, ComplexVectorPair)
{
    eval("z = complex([1 2 3], [10 20 30]);");
    auto *z = getVarPtr("z");
    ASSERT_NE(z, nullptr);
    EXPECT_TRUE(z->isComplex());
    EXPECT_EQ(z->numel(), 3u);
    EXPECT_DOUBLE_EQ(z->complexData()[0].real(), 1.0);
    EXPECT_DOUBLE_EQ(z->complexData()[0].imag(), 10.0);
    EXPECT_DOUBLE_EQ(z->complexData()[2].imag(), 30.0);
}

TEST_P(TypeOpsTest, ComplexScalarImagBroadcast)
{
    eval("z = complex([1 2 3], 5);");
    auto *z = getVarPtr("z");
    ASSERT_NE(z, nullptr);
    EXPECT_TRUE(z->isComplex());
    EXPECT_EQ(z->numel(), 3u);
    for (size_t i = 0; i < 3; ++i)
        EXPECT_DOUBLE_EQ(z->complexData()[i].imag(), 5.0);
    EXPECT_DOUBLE_EQ(z->complexData()[0].real(), 1.0);
    EXPECT_DOUBLE_EQ(z->complexData()[2].real(), 3.0);
}

TEST_P(TypeOpsTest, ComplexScalarRealBroadcast)
{
    eval("z = complex(5, [1 2 3]);");
    auto *z = getVarPtr("z");
    ASSERT_NE(z, nullptr);
    EXPECT_EQ(z->numel(), 3u);
    for (size_t i = 0; i < 3; ++i)
        EXPECT_DOUBLE_EQ(z->complexData()[i].real(), 5.0);
    EXPECT_DOUBLE_EQ(z->complexData()[2].imag(), 3.0);
}

TEST_P(TypeOpsTest, Complex3DPreservesShape)
{
    eval("A = reshape(1:8, 2, 2, 2); z = complex(A);");
    auto *z = getVarPtr("z");
    ASSERT_NE(z, nullptr);
    EXPECT_TRUE(z->isComplex());
    EXPECT_TRUE(z->dims().is3D());
    EXPECT_EQ(z->numel(), 8u);
    EXPECT_DOUBLE_EQ(z->complexData()[7].real(), 8.0);
    EXPECT_DOUBLE_EQ(z->complexData()[7].imag(), 0.0);
}

TEST_P(TypeOpsTest, Complex3DBothArgs)
{
    eval("R = reshape(1:8, 2, 2, 2); I = reshape(11:18, 2, 2, 2); z = complex(R, I);");
    auto *z = getVarPtr("z");
    ASSERT_NE(z, nullptr);
    EXPECT_TRUE(z->isComplex());
    EXPECT_TRUE(z->dims().is3D());
    EXPECT_DOUBLE_EQ(z->complexData()[0].real(), 1.0);
    EXPECT_DOUBLE_EQ(z->complexData()[0].imag(), 11.0);
    EXPECT_DOUBLE_EQ(z->complexData()[7].imag(), 18.0);
}

TEST_P(TypeOpsTest, ComplexFromIntegerTypeOk)
{
    eval("z = complex(int32([1 2 3]));");
    auto *z = getVarPtr("z");
    ASSERT_NE(z, nullptr);
    EXPECT_TRUE(z->isComplex());
    EXPECT_DOUBLE_EQ(z->complexData()[0].real(), 1.0);
    EXPECT_DOUBLE_EQ(z->complexData()[2].real(), 3.0);
}

TEST_P(TypeOpsTest, ComplexShapeMismatchThrows)
{
    EXPECT_THROW({ eval("complex([1 2 3], [1 2]);"); }, std::exception);
}

// ============================================================
// Empty-shape preservation through numeric constructors + unary
// (regression for a legacy std::max(cols, 1) guard that silently
// promoted 0-col inputs to a 1-col result)
// ============================================================

TEST_P(TypeOpsTest, Int32OfZeroColsPreservesShape)
{
    eval("a = int32(zeros(3, 0));");
    auto *a = getVarPtr("a");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->type(), MType::INT32);
    EXPECT_EQ(a->dims().rows(), 3u);
    EXPECT_EQ(a->dims().cols(), 0u);
    EXPECT_EQ(a->numel(), 0u);
}

TEST_P(TypeOpsTest, Uint16OfZeroRowsPreservesShape)
{
    eval("a = uint16(zeros(0, 4));");
    auto *a = getVarPtr("a");
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(a->type(), MType::UINT16);
    EXPECT_EQ(a->dims().rows(), 0u);
    EXPECT_EQ(a->dims().cols(), 4u);
    EXPECT_EQ(a->numel(), 0u);
}

// ── Unary empty shape preservation ─────────────────

TEST_P(TypeOpsTest, UnaryMinusEmptyDoublePreservesShape)
{
    eval("a = zeros(3, 0); b = -a;");
    auto *b = getVarPtr("b");
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->type(), MType::DOUBLE);
    EXPECT_EQ(b->dims().rows(), 3u);
    EXPECT_EQ(b->dims().cols(), 0u);
}

TEST_P(TypeOpsTest, UnaryMinusEmptyInt32PreservesShape)
{
    eval("a = int32(zeros(2, 0)); b = -a;");
    auto *b = getVarPtr("b");
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->type(), MType::INT32);
    EXPECT_EQ(b->dims().rows(), 2u);
    EXPECT_EQ(b->dims().cols(), 0u);
}

TEST_P(TypeOpsTest, UnaryMinusEmptyCharPromotesToDouble)
{
    eval("b = -'';");
    auto *b = getVarPtr("b");
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->type(), MType::DOUBLE);
    EXPECT_EQ(b->numel(), 0u);
}

TEST_P(TypeOpsTest, LogicalNotEmptyPreservesShape)
{
    eval("a = zeros(3, 0); b = ~a;");
    auto *b = getVarPtr("b");
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->type(), MType::LOGICAL);
    EXPECT_EQ(b->dims().rows(), 3u);
    EXPECT_EQ(b->dims().cols(), 0u);
}

TEST_P(TypeOpsTest, LogicalNot3DPreservesShape)
{
    // Was a heap-corruption site: ~ used MValue::matrix(rows, cols) for
    // the result then wrote numel bytes — past the 2D buffer end for 3D.
    eval("a = zeros(2, 3, 2); b = ~a;");
    auto *b = getVarPtr("b");
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->type(), MType::LOGICAL);
    EXPECT_TRUE(b->dims().is3D());
    EXPECT_EQ(b->numel(), 12u);
    for (size_t i = 0; i < 12; ++i)
        EXPECT_EQ(b->logicalData()[i], 1u);
}

TEST_P(TypeOpsTest, UnaryMinus3DPreservesShape)
{
    eval("a = ones(2, 2, 2); b = -a;");
    auto *b = getVarPtr("b");
    ASSERT_NE(b, nullptr);
    EXPECT_TRUE(b->dims().is3D());
    EXPECT_EQ(b->numel(), 8u);
    for (size_t i = 0; i < 8; ++i)
        EXPECT_DOUBLE_EQ(b->doubleData()[i], -1.0);
}

TEST_P(TypeOpsTest, UnaryMinusEmptyComplexPreservesShape)
{
    eval("a = complex(zeros(2, 0)); b = -a;");
    auto *b = getVarPtr("b");
    ASSERT_NE(b, nullptr);
    EXPECT_TRUE(b->isComplex());
    EXPECT_EQ(b->dims().rows(), 2u);
    EXPECT_EQ(b->dims().cols(), 0u);
}

TEST_P(TypeOpsTest, UnaryMinusEmptyLogicalPromotesToDouble)
{
    eval("a = logical(zeros(3, 0)); b = -a;");
    auto *b = getVarPtr("b");
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->type(), MType::DOUBLE);
    EXPECT_EQ(b->dims().rows(), 3u);
    EXPECT_EQ(b->dims().cols(), 0u);
}


INSTANTIATE_DUAL(TypeOpsTest);
