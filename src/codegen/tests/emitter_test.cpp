// codegen/tests/emitter_test.cpp
//
// Unit tests for the C++ emitter — sub-brick 3a: unboxed-scalar
// expressions. Parse a scalar expression, emit C++, assert the string.
// (String-level verification; end-to-end compile+run+diff arrives with
// the AOT harness, brick 4.)

#include <numkit/codegen/emitter.hpp>

#include <numkit/core/lexer.hpp>
#include <numkit/core/parser.hpp>

#include <gtest/gtest.h>

using numkit::ValueType;
using namespace numkit::codegen;

namespace {

// Parse "__e = (<expr>);" and emit the right-hand side expression.
std::string emit(const std::string &expr)
{
    numkit::Lexer  lex("__e = (" + expr + ");");
    auto           toks = lex.tokenize();
    numkit::Parser parser(toks);
    auto           root = parser.parse();
    const numkit::ASTNode &assign = *root->children.back();  // ASSIGN
    return emitScalarExpr(*assign.children[1]);               // rhs
}

bool contains(const std::string &hay, const std::string &needle)
{
    return hay.find(needle) != std::string::npos;
}

// Parse `src` and return the first FUNCTION_DEF (kept alive via `root`).
const numkit::ASTNode *findFunc(const std::string &src, numkit::ASTNodePtr &root)
{
    numkit::Lexer  lex(src);
    numkit::Parser parser(lex.tokenize());
    root = parser.parse();
    for (const auto &c : root->children)
        if (c && c->type == numkit::NodeType::FUNCTION_DEF) return c.get();
    return nullptr;
}

// A standard registry for the function-level tests.
TransferRegistry stdReg()
{
    TransferRegistry reg;
    registerStandardTransfers(reg);
    return reg;
}

const InferredType kDoubleScalar = InferredType::scalar(ValueType::DOUBLE);
const InferredType kDoubleRow    =
    InferredType::concrete(ValueType::DOUBLE, numkit::codegen::Shape::rowVector());

const char *kBiquadSrc =
    "function y = biquad(x, b0, b1, b2, a1, a2)\n"
    "  n = numel(x);\n"
    "  y = zeros(1, n);\n"
    "  x1 = 0; x2 = 0; y1 = 0; y2 = 0;\n"
    "  for k = 1:n\n"
    "    xn = x(k);\n"
    "    yn = b0*xn + b1*x1 + b2*x2 - a1*y1 - a2*y2;\n"
    "    y(k) = yn;\n"
    "    x2 = x1; x1 = xn;\n"
    "    y2 = y1; y1 = yn;\n"
    "  end\n"
    "end\n";

}  // namespace

TEST(Emitter, ScalarType)
{
    EXPECT_EQ(cppScalarType(ValueType::DOUBLE), "double");
    EXPECT_EQ(cppScalarType(ValueType::SINGLE), "float");
    EXPECT_EQ(cppScalarType(ValueType::COMPLEX), "std::complex<double>");
    EXPECT_EQ(cppScalarType(ValueType::LOGICAL), "bool");
    EXPECT_EQ(cppScalarType(ValueType::INT32), "std::int32_t");
    EXPECT_THROW(cppScalarType(ValueType::CELL), std::runtime_error);
}

TEST(Emitter, DoubleLiteralFormat)
{
    EXPECT_EQ(formatDoubleLiteral(0.0675), "0.0675");
    EXPECT_EQ(formatDoubleLiteral(2.0), "2.0");      // integer-valued -> .0
    EXPECT_EQ(formatDoubleLiteral(-1.143), "-1.143");
    EXPECT_EQ(formatDoubleLiteral(0.0), "0.0");
}

TEST(Emitter, NumberAndIdentifier)
{
    EXPECT_EQ(emit("0.0675"), "0.0675");
    EXPECT_EQ(emit("xn"), "xn");
}

TEST(Emitter, ScalarArithmetic)
{
    EXPECT_EQ(emit("b0*xn"), "(b0 * xn)");
    EXPECT_EQ(emit("b0*xn + b1*x1"), "((b0 * xn) + (b1 * x1))");
}

// The biquad inner update emits as a plain nested C++ double expression.
TEST(Emitter, BiquadBody)
{
    // left-associative: ((((b0*xn + b1*x1) + b2*x2) - a1*y1) - a2*y2)
    EXPECT_EQ(emit("b0*xn + b1*x1 + b2*x2 - a1*y1 - a2*y2"),
              "(((((b0 * xn) + (b1 * x1)) + (b2 * x2)) - (a1 * y1)) - (a2 * y2))");
}

TEST(Emitter, PowerLowersToStdPow)
{
    EXPECT_EQ(emit("2^3"), "std::pow(2.0, 3.0)");
    EXPECT_EQ(emit("x.^2"), "std::pow(x, 2.0)");
}

TEST(Emitter, ComparisonAndLogical)
{
    EXPECT_EQ(emit("a < b"), "(a < b)");
    EXPECT_EQ(emit("a == b"), "(a == b)");
    EXPECT_EQ(emit("a ~= b"), "(a != b)");
}

TEST(Emitter, Unary)
{
    EXPECT_EQ(emit("-x"), "(-x)");
    EXPECT_EQ(emit("~b"), "(!b)");
}

TEST(Emitter, ImaginaryLiteral)
{
    EXPECT_EQ(emit("2i"), "std::complex<double>(0.0, 2.0)");
}

// A construct outside this sub-brick (a call) throws rather than emitting
// something wrong — the boundary is explicit.
TEST(Emitter, UnsupportedThrows)
{
    EXPECT_THROW(emit("foo(x)"), std::runtime_error);
}

// ── 3b: declaration-type prepass ──────────────────────────────────────
TEST(EmitterFn, DeclTypesBiquad)
{
    const auto reg = stdReg();
    numkit::ASTNodePtr root;
    const numkit::ASTNode *fn = findFunc(kBiquadSrc, root);
    ASSERT_NE(fn, nullptr);

    TypeEnv entry;
    entry.set("x", {kDoubleRow, ConstVal::unknown()});
    for (const char *p : {"b0", "b1", "b2", "a1", "a2"})
        entry.set(p, {kDoubleScalar, ConstVal::unknown()});

    const DeclTypeMap dt = computeDeclTypes(*fn->children[0], entry, reg);

    // y is the double output array; the loop carries scalar doubles.
    ASSERT_TRUE(dt.count("y"));
    EXPECT_TRUE(dt.at("y").isConcrete());
    EXPECT_EQ(dt.at("y").dtype, ValueType::DOUBLE);
    EXPECT_FALSE(dt.at("y").shape.isScalar());
    for (const char *v : {"n", "k", "x1", "x2", "y1", "y2", "xn", "yn"}) {
        ASSERT_TRUE(dt.count(v)) << v;
        EXPECT_TRUE(dt.at(v).isUnboxableScalar()) << v;
        EXPECT_EQ(dt.at(v).dtype, ValueType::DOUBLE) << v;
    }
}

// ── 3f: whole-function emission ───────────────────────────────────────
TEST(EmitterFn, ScalarReturnSignature)
{
    const auto reg = stdReg();
    numkit::ASTNodePtr root;
    const numkit::ASTNode *fn =
        findFunc("function y = f(a, b)\n  y = a + b;\nend\n", root);
    ASSERT_NE(fn, nullptr);

    const EmittedFunction out = emitFunction(
        *fn, {{"a", kDoubleScalar}, {"b", kDoubleScalar}}, reg);

    EXPECT_EQ(out.signature, "double f(double a, double b)");
    EXPECT_TRUE(contains(out.source, "double y = 0.0;"));  // hoisted local
    EXPECT_TRUE(contains(out.source, "y = (a + b);"));
    EXPECT_TRUE(contains(out.source, "return y;"));
    EXPECT_TRUE(contains(out.source, "#include <cmath>"));  // self-contained prelude
}

// ---- native math coverage --------------------------------------------------
// erf/erfc/expm1/asinh are total on R and lower straight to std:: — no bridge,
// the TU stays self-contained.
TEST(EmitterFn, RealOnlyMathLowersToStd)
{
    const auto             reg = stdReg();
    numkit::ASTNodePtr     root;
    const numkit::ASTNode *fn = findFunc("function y = f(x)\n  y = erf(x);\nend\n", root);
    ASSERT_NE(fn, nullptr);
    const EmittedFunction out = emitFunction(*fn, {{"x", kDoubleScalar}}, reg);
    EXPECT_TRUE(contains(out.source, "std::erf("));
    EXPECT_FALSE(contains(out.source, "nk_codegen_rt.h"));  // self-contained, no bridge
}

// A complex argument has no std::erf overload, so the transfer refuses it
// (Dynamic) and — without bridging — the emitter throws rather than emit
// non-compiling std::erf(std::complex).
TEST(EmitterFn, ComplexErfRefusedWithoutBridge)
{
    const auto             reg = stdReg();
    numkit::ASTNodePtr     root;
    const numkit::ASTNode *fn = findFunc("function y = f(x)\n  y = erf(x);\nend\n", root);
    ASSERT_NE(fn, nullptr);
    EXPECT_THROW(emitFunction(*fn, {{"x", InferredType::scalar(ValueType::COMPLEX)}}, reg),
                 std::runtime_error);
}

// ---- bridged emission (DESIGN.md §6a) --------------------------------------
// `sign` is typed scalar->scalar by the registry (realMathUnaryTransfer) but
// has no clean std form, so the emitter cannot lower it. Opt-in bridging emits
// it as a C-ABI call into the runtime instead of throwing.

TEST(EmitterFn, BridgedScalarCallWhenEnabled)
{
    const auto             reg = stdReg();
    numkit::ASTNodePtr     root;
    const numkit::ASTNode *fn = findFunc("function y = f(x)\n  y = sign(x);\nend\n", root);
    ASSERT_NE(fn, nullptr);

    const BridgeOptions   bridge{true, "nk_codegen_rt.h"};
    const EmittedFunction out = emitFunction(*fn, {{"x", kDoubleScalar}}, reg, nullptr, bridge);

    EXPECT_TRUE(contains(out.source, "nk_rt::bridge_scalar(\"sign\", {x})"));  // the bridged call
    EXPECT_TRUE(contains(out.source, "#include \"nk_codegen_rt.h\""));         // runtime header
    EXPECT_TRUE(contains(out.source, "inline double bridge_scalar("));         // helper present
}

// Opt-in / self-contained contract: with bridging OFF (the default) an
// un-lowerable call throws — the TU never silently pulls in the runtime.
TEST(EmitterFn, UnlowerableCallThrowsWhenBridgingDisabled)
{
    const auto             reg = stdReg();
    numkit::ASTNodePtr     root;
    const numkit::ASTNode *fn = findFunc("function y = f(x)\n  y = sign(x);\nend\n", root);
    ASSERT_NE(fn, nullptr);
    EXPECT_THROW(emitFunction(*fn, {{"x", kDoubleScalar}}, reg), std::runtime_error);
}

// Bridging is a FALLBACK, not a hijack: a builtin the emitter CAN lower
// (numel here) is still lowered natively even with bridging enabled — no
// bridged call is emitted.
TEST(EmitterFn, LowerableBuiltinsNotBridgedWhenEnabled)
{
    const auto             reg = stdReg();
    numkit::ASTNodePtr     root;
    const numkit::ASTNode *fn = findFunc(kBiquadSrc, root);
    ASSERT_NE(fn, nullptr);
    const BridgeOptions bridge{true, "nk_codegen_rt.h"};
    const std::string   s =
        emitFunction(*fn,
                     {{"x", kDoubleRow}, {"b0", kDoubleScalar}, {"b1", kDoubleScalar},
                      {"b2", kDoubleScalar}, {"a1", kDoubleScalar}, {"a2", kDoubleScalar}},
                     reg, nullptr, bridge)
            .source;
    EXPECT_FALSE(contains(s, "bridge_scalar(\""));  // a CALL; the helper def has no quote
}

TEST(EmitterFn, BiquadFullFunction)
{
    const auto reg = stdReg();
    numkit::ASTNodePtr root;
    const numkit::ASTNode *fn = findFunc(kBiquadSrc, root);
    ASSERT_NE(fn, nullptr);

    std::vector<ParamSpec> params = {{"x", kDoubleRow}};
    for (const char *p : {"b0", "b1", "b2", "a1", "a2"})
        params.push_back({p, kDoubleScalar});

    const EmittedFunction out = emitFunction(*fn, params, reg);
    const std::string &s = out.source;

    // RawBuffer ABI signature: array param -> ptr + len; output -> trailing
    // out-param; scalars unboxed; void return.
    EXPECT_EQ(out.signature,
              "void biquad(const double* x, std::size_t x_len, double b0, "
              "double b1, double b2, double a1, double a2, double* y, "
              "std::size_t y_len)");

    // hoisted scalar locals (k is the promoted counter -> declared in the
    // for, not hoisted; see brick 6 below)
    for (const char *v : {"n", "x1", "x2", "xn", "y1", "y2", "yn"})
        EXPECT_TRUE(contains(s, std::string("double ") + v + " = 0.0;")) << v;

    EXPECT_TRUE(contains(s, "n = static_cast<double>(x_len);"));        // numel(x)
    EXPECT_TRUE(contains(s, "for (std::size_t __i = 0; __i < y_len; ++__i)"));  // y = zeros(1,n)
    EXPECT_TRUE(contains(s, "y[__i] = 0.0;"));
    EXPECT_TRUE(contains(
        s, "yn = (((((b0 * xn) + (b1 * x1)) + (b2 * x2)) - (a1 * y1)) - (a2 * y2));"));
    EXPECT_TRUE(contains(s, "x2 = x1;"));
    EXPECT_TRUE(contains(s, "y1 = yn;"));
}

// ── 6: clean-index loop promotion (gated, deletable) ──────────────────
// The biquad loop is the canonical case: k used only as a 1-based index of
// arrays whose element count is provably the loop extent -> a 0-based
// size_t counter, unchecked A[k], no double loop var, no -1.
TEST(EmitterFn, BiquadPromotedLoop)
{
    const auto reg = stdReg();
    numkit::ASTNodePtr root;
    const numkit::ASTNode *fn = findFunc(kBiquadSrc, root);
    ASSERT_NE(fn, nullptr);

    std::vector<ParamSpec> params = {{"x", kDoubleRow}};
    for (const char *p : {"b0", "b1", "b2", "a1", "a2"})
        params.push_back({p, kDoubleScalar});
    const std::string s = emitFunction(*fn, params, reg).source;

    EXPECT_TRUE(contains(s, "for (std::size_t k = 0; k < x_len; ++k)"));
    EXPECT_TRUE(contains(s, "xn = x[k];"));
    EXPECT_TRUE(contains(s, "y[k] = yn;"));
    // the optimisation fired: no checked access, no double loop var
    EXPECT_FALSE(contains(s, "nk_rt::index"));
    EXPECT_FALSE(contains(s, "double k = 0.0;"));
    EXPECT_FALSE(contains(s, "k <= n"));
}

// When the gate does NOT hold (k used in arithmetic), the emitter must
// fall back to the always-correct checked form (no-kludge litmus).
TEST(EmitterFn, NonPromotableLoopFallsBack)
{
    const auto reg = stdReg();
    numkit::ASTNodePtr root;
    const numkit::ASTNode *fn = findFunc(
        "function y = f(x)\n"
        "  n = numel(x);\n  y = zeros(1, n);\n"
        "  for k = 1:n\n    y(k) = x(k) + k;\n  end\n"  // `+ k` -> not clean-index
        "end\n",
        root);
    ASSERT_NE(fn, nullptr);

    const std::string s = emitFunction(*fn, {{"x", kDoubleRow}}, reg).source;
    EXPECT_TRUE(contains(s, "double k = 0.0;"));                 // hoisted double loop var
    EXPECT_TRUE(contains(s, "for (k = 1.0; k <= n; k += 1.0)"));  // checked counted loop
    EXPECT_TRUE(contains(s, "nk_rt::index(x, x_len, k)"));        // checked read
    EXPECT_TRUE(contains(s, "nk_rt::index_set(y, y_len, k,"));    // checked write
    EXPECT_FALSE(contains(s, "for (std::size_t k = 0;"));        // NOT promoted
}

// SOUNDNESS: the loop bound `n` is reassigned after `n = numel(x)`, so the
// numel-equality fact is stale — the loop must NOT promote (promotion
// would emit `k < x_len`, the wrong count, and could write the output out
// of bounds). The flow-sensitive fact store invalidates on reassignment.
TEST(EmitterFn, ReassignedBoundNotPromoted)
{
    const auto reg = stdReg();
    numkit::ASTNodePtr root;
    const numkit::ASTNode *fn = findFunc(
        "function y = f(x)\n"
        "  n = numel(x);\n  n = 3;\n  y = zeros(1, n);\n"
        "  for k = 1:n\n    y(k) = x(k);\n  end\n"
        "end\n",
        root);
    ASSERT_NE(fn, nullptr);

    const std::string s = emitFunction(*fn, {{"x", kDoubleRow}}, reg).source;
    EXPECT_FALSE(contains(s, "for (std::size_t k = 0;"));         // NOT promoted (stale fact)
    EXPECT_TRUE(contains(s, "for (k = 1.0; k <= n; k += 1.0)"));   // checked form, bound n
    EXPECT_TRUE(contains(s, "nk_rt::index_set(y, y_len, k,"));
}

// ── 3c: control flow ──────────────────────────────────────────────────
TEST(EmitterFn, IfElseTyping)
{
    const auto reg = stdReg();
    numkit::ASTNodePtr root;
    const numkit::ASTNode *fn = findFunc(
        "function y = pick(c, a, b)\n"
        "  if c\n    y = a;\n  else\n    y = b;\n  end\n"
        "end\n",
        root);
    ASSERT_NE(fn, nullptr);

    const EmittedFunction out = emitFunction(
        *fn, {{"c", kDoubleScalar}, {"a", kDoubleScalar}, {"b", kDoubleScalar}}, reg);
    const std::string &s = out.source;

    EXPECT_EQ(out.signature, "double pick(double c, double a, double b)");
    EXPECT_TRUE(contains(s, "if (c) {"));
    EXPECT_TRUE(contains(s, "y = a;"));
    EXPECT_TRUE(contains(s, "} else {") || contains(s, "else {"));
    EXPECT_TRUE(contains(s, "y = b;"));
    EXPECT_TRUE(contains(s, "return y;"));
}

TEST(EmitterFn, WhileLoop)
{
    const auto reg = stdReg();
    numkit::ASTNodePtr root;
    const numkit::ASTNode *fn = findFunc(
        "function y = acc(n)\n"
        "  y = 0;\n  i = 1;\n  while i <= n\n    y = y + i;\n    i = i + 1;\n  end\n"
        "end\n",
        root);
    ASSERT_NE(fn, nullptr);

    const EmittedFunction out = emitFunction(*fn, {{"n", kDoubleScalar}}, reg);
    EXPECT_TRUE(contains(out.source, "while ((i <= n))"));
    EXPECT_TRUE(contains(out.source, "y = (y + i);"));
}

// ── 3d: builtin call lowering ─────────────────────────────────────────
TEST(EmitterFn, BuiltinMathLowering)
{
    const auto reg = stdReg();
    numkit::ASTNodePtr root;
    const numkit::ASTNode *fn = findFunc(
        "function y = g(x)\n  y = sin(x) + cos(x) + tan(x) + exp(x) + floor(x) + fix(x);\nend\n",
        root);
    ASSERT_NE(fn, nullptr);

    const std::string s = emitFunction(*fn, {{"x", kDoubleScalar}}, reg).source;
    EXPECT_TRUE(contains(s, "std::sin(x)"));
    EXPECT_TRUE(contains(s, "std::cos(x)"));
    EXPECT_TRUE(contains(s, "std::tan(x)"));
    EXPECT_TRUE(contains(s, "std::exp(x)"));
    EXPECT_TRUE(contains(s, "std::floor(x)"));
    EXPECT_TRUE(contains(s, "std::trunc(x)"));  // MATLAB fix -> std::trunc
}

// A builtin that the transfer registry CAN type (sign -> scalar double)
// but the emitter does not lower hits the explicit boundary and throws —
// no silent wrong code, and (post dead-code trim) no unreachable lowering.
TEST(EmitterFn, TypedButUnloweredBuiltinThrows)
{
    const auto reg = stdReg();
    numkit::ASTNodePtr root;
    const numkit::ASTNode *fn = findFunc("function y = g(x)\n  y = sign(x);\nend\n", root);
    ASSERT_NE(fn, nullptr);
    EXPECT_THROW(emitFunction(*fn, {{"x", kDoubleScalar}}, reg), std::runtime_error);
}

// ── 2-D matrix indexing emission ──────────────────────────────────────
TEST(EmitterFn, Matrix2DIndexEmission)
{
    const auto reg = stdReg();
    numkit::ASTNodePtr root;
    const numkit::ASTNode *fn =
        findFunc("function s = tr(A)\n  s = A(1,1) + A(3,2);\nend\n", root);
    ASSERT_NE(fn, nullptr);

    const std::string s = emitFunction(
        *fn, {{"A", InferredType::concrete(ValueType::DOUBLE, numkit::codegen::Shape::dims(3, 3))}},
        reg).source;

    EXPECT_TRUE(contains(s, "const double* A, std::size_t A_rows, std::size_t A_cols"));
    EXPECT_TRUE(contains(s, "nk_rt::index2(A, A_rows, A_cols, 1.0, 1.0)"));
    EXPECT_TRUE(contains(s, "nk_rt::index2(A, A_rows, A_cols, 3.0, 2.0)"));
}

// ── boundary #2b: multi-output emission ───────────────────────────────
TEST(EmitterFn, MultiOutputEmission)
{
    const char *src =
        "function [a, b] = two(x)\n  a = x + 1;\n  b = x * 2;\nend\n"
        "function y = run(x)\n  [p, q] = two(x);\n  y = p + q;\nend\n";
    numkit::Lexer  lex(src);
    numkit::Parser parser(lex.tokenize());
    auto           root = parser.parse();
    FunctionTable  ft;
    collectFunctions(*root, ft);
    TransferRegistry reg;
    registerStandardTransfers(reg);
    registerUserFunctions(reg, ft);

    const EmittedFunction out =
        emitProgram(*ft.find("run"), {{"x", kDoubleScalar}}, ft, reg);
    const std::string &s = out.source;

    // multi-output -> void + reference out-params
    EXPECT_TRUE(contains(s, "void two__d(double x, double& a, double& b)"));
    EXPECT_TRUE(contains(s, "two__d(x, p, q);"));   // targets appended as out-args
    EXPECT_TRUE(contains(s, "double p = 0.0;"));    // targets hoisted in the caller
    EXPECT_TRUE(contains(s, "double q = 0.0;"));
}

// ── class brick 5a: struct emission ───────────────────────────────────
TEST(EmitterFn, ClassStructEmission)
{
    const char *src = "classdef Point\n  properties\n    x = 0\n    y = 5\n  end\nend\n";
    numkit::Lexer  lex(src);
    numkit::Parser p(lex.tokenize());
    auto           root = p.parse();
    const numkit::ASTNode *cd = nullptr;
    for (const auto &c : root->children)
        if (c && c->type == numkit::NodeType::CLASSDEF_DEF) cd = c.get();
    ASSERT_NE(cd, nullptr);

    const auto reg = stdReg();
    const ClassInfo ci = buildClassInfo(*cd, 0, reg);
    const std::string s = emitClassStruct(ci);

    EXPECT_TRUE(contains(s, "struct Point {"));
    EXPECT_TRUE(contains(s, "double x = 0.0;"));
    EXPECT_TRUE(contains(s, "double y = 5.0;"));  // the actual default, not zero
}

// ── class brick 5b: field access + object params/returns ─────────────
TEST(EmitterFn, ValueClassFieldAccess)
{
    const char *src =
        "classdef Point\n  properties\n    x = 0\n    y = 0\n  end\nend\n"
        "function p = setx(p, v)\n  p.x = v;\nend\n";
    numkit::Lexer  lex(src);
    numkit::Parser parser(lex.tokenize());
    auto           root = parser.parse();

    const auto    reg = stdReg();
    ClassRegistry creg;
    collectClasses(*root, creg, reg);
    ASSERT_TRUE(creg.has("Point"));
    const int id = creg.idOf("Point");

    const numkit::ASTNode *fn = nullptr;
    for (const auto &c : root->children)
        if (c && c->type == numkit::NodeType::FUNCTION_DEF) fn = c.get();
    ASSERT_NE(fn, nullptr);

    const EmittedFunction out = emitFunction(
        *fn, {{"p", InferredType::object(id)}, {"v", kDoubleScalar}}, reg, &creg);
    const std::string &s = out.source;

    EXPECT_EQ(out.signature, "Point setx(Point p, double v)");  // object in + out, by value
    EXPECT_TRUE(contains(s, "struct Point {"));                 // struct emitted
    EXPECT_TRUE(contains(s, "double x = 0.0;"));
    EXPECT_TRUE(contains(s, "p.x = v;"));                       // field write (value -> '.')
    EXPECT_TRUE(contains(s, "return p;"));
}

// ── class brick 6: monomorphic method calls ──────────────────────────
TEST(EmitterFn, MethodCallEmission)
{
    const char *src =
        "classdef Rect\n  properties\n    w = 0\n    h = 0\n  end\n"
        "  methods\n    function a = area(obj)\n      a = obj.w * obj.h;\n    end\n  end\nend\n"
        "function y = run(p)\n  y = p.area();\nend\n";
    numkit::Lexer  lex(src);
    numkit::Parser parser(lex.tokenize());
    auto           root = parser.parse();

    TransferRegistry reg;
    registerStandardTransfers(reg);
    ClassRegistry creg;
    collectClasses(*root, creg, reg);
    FunctionTable ft;
    collectFunctions(*root, ft);
    registerUserFunctions(reg, ft);
    registerClassMethods(reg, creg);

    const int id = creg.idOf("Rect");
    ASSERT_GE(id, 0);
    const EmittedFunction out =
        emitProgram(*ft.find("run"), {{"p", InferredType::object(id)}}, ft, reg, &creg);
    const std::string &s = out.source;

    EXPECT_EQ(out.name, "run__o0");
    EXPECT_TRUE(contains(s, "struct Rect {"));
    EXPECT_TRUE(contains(s, "double Rect__area__o0(Rect obj)"));  // method specialisation
    EXPECT_TRUE(contains(s, "a = (obj.w * obj.h);"));            // field reads in the body
    EXPECT_TRUE(contains(s, "Rect__area__o0(p)"));               // call site (self = p)
}

// ── engine 1b: interprocedural call emission ──────────────────────────
TEST(EmitterFn, InterproceduralProgram)
{
    const char *src =
        "function y = f(x)\n  y = g(x) + 1;\nend\n"
        "function y = g(x)\n  y = x*2;\nend\n";
    numkit::Lexer  lex(src);
    numkit::Parser parser(lex.tokenize());
    auto           root = parser.parse();

    FunctionTable table;
    collectFunctions(*root, table);
    TransferRegistry reg;
    registerStandardTransfers(reg);
    registerUserFunctions(reg, table);

    const numkit::ASTNode *f = table.find("f");
    ASSERT_NE(f, nullptr);
    const EmittedFunction out = emitProgram(*f, {{"x", kDoubleScalar}}, table, reg);
    const std::string    &s   = out.source;

    EXPECT_EQ(out.name, "f__d");                              // mangled entry symbol
    EXPECT_TRUE(contains(s, "double f__d(double x);"));       // forward decls
    EXPECT_TRUE(contains(s, "double g__d(double x);"));
    EXPECT_TRUE(contains(s, "double f__d(double x) {"));      // definitions
    EXPECT_TRUE(contains(s, "double g__d(double x) {"));
    EXPECT_TRUE(contains(s, "g__d(x)"));                      // f calls the specialisation
    EXPECT_TRUE(contains(s, "y = (g__d(x) + 1.0);"));
}

// An array variable passed across an interprocedural call (boundary #3) is
// emitted as `ptr, len` (its length companion), and the callee
// specialisation takes the array param.
TEST(EmitterFn, InterproceduralArrayArgPassed)
{
    const char *src =
        "function y = f(v)\n  y = g(v);\nend\n"
        "function y = g(v)\n  y = numel(v);\nend\n";
    numkit::Lexer  lex(src);
    numkit::Parser parser(lex.tokenize());
    auto           root = parser.parse();
    FunctionTable  table;
    collectFunctions(*root, table);
    TransferRegistry reg;
    registerStandardTransfers(reg);
    registerUserFunctions(reg, table);

    const EmittedFunction out = emitProgram(*table.find("f"), {{"v", kDoubleRow}}, table, reg);
    EXPECT_TRUE(contains(out.source, "g__dr(v, v_len)"));            // ptr + len at the call site
    EXPECT_TRUE(contains(out.source, "double g__dr(const double* v, std::size_t v_len)"));
}

// A construct that infers to Dynamic (eval) cannot be typed -> the output
// type is unsupported and emission refuses (Contract 2: never emit wrong
// code).
TEST(EmitterFn, DynamicOutputRefused)
{
    const auto reg = stdReg();
    numkit::ASTNodePtr root;
    const numkit::ASTNode *fn =
        findFunc("function y = h(x)\n  y = eval(x);\nend\n", root);
    ASSERT_NE(fn, nullptr);
    EXPECT_THROW(emitFunction(*fn, {{"x", kDoubleScalar}}, reg), std::runtime_error);
}