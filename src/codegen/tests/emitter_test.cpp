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

// CX1: complex scalar accessors. real/imag/angle -> real (std::real/imag/arg);
// conj -> complex (std::conj), but conj of a REAL value is the identity (a bare
// std::conj(double) would return a std::complex, mismatching the transfer).
TEST(EmitterFn, ComplexAccessors)
{
    const auto         reg = stdReg();
    const InferredType cx  = InferredType::scalar(ValueType::COMPLEX);
    auto body = [&](const char *expr, const InferredType &xt) {
        numkit::ASTNodePtr     root;
        const std::string      src = std::string("function y = f(x)\n  y = ") + expr + ";\nend\n";
        const numkit::ASTNode *fn  = findFunc(src, root);
        return emitFunction(*fn, {{"x", xt}}, reg).source;
    };
    EXPECT_TRUE(contains(body("real(x)", cx), "y = std::real(x);"));   // -> double
    EXPECT_TRUE(contains(body("imag(x)", cx), "y = std::imag(x);"));
    EXPECT_TRUE(contains(body("angle(x)", cx), "y = std::arg(x);"));
    EXPECT_TRUE(contains(body("conj(x)", cx), "y = std::conj(x);"));   // -> complex
    EXPECT_TRUE(contains(body("conj(x)", kDoubleScalar), "y = (x);"));  // real conj = identity
}

// CX2/CX3: complex 1-D arrays. Params/indexing/numel already flow through the
// dtype-agnostic buffer machinery (`const std::complex<double>* x`); the gap was
// elementwise — now lifted (the fill loop is valid std::complex arithmetic).
TEST(EmitterFn, ComplexElementwise)
{
    const auto             reg    = stdReg();
    const InferredType     cxrow  = InferredType::concrete(ValueType::COMPLEX, Shape::rowVector());
    numkit::ASTNodePtr     root;
    const numkit::ASTNode *fn = findFunc("function y = f(x)\n  y = x .* 2;\nend\n", root);
    ASSERT_NE(fn, nullptr);
    const std::string s = emitFunction(*fn, {{"x", cxrow}}, reg).source;
    EXPECT_TRUE(contains(s, "const std::complex<double>* x"));            // complex buffer param
    EXPECT_TRUE(contains(s, "std::complex<double>* __restrict y"));       // complex out-param
    EXPECT_TRUE(contains(s, "y[_nk_i] = (x[_nk_i] * 2.0);"));             // std::complex fill loop
}

// CX5: complex 2-D / N-D arrays flow through the dtype-agnostic buffer + index
// machinery — params (const std::complex<double>* A, dims) and indexed reads
// (index2 / indexN are templated on T). (2-D/N-D elementwise is a separate,
// non-complex gap — the elementwise fill loop is 1-D only.)
TEST(EmitterFn, ComplexMatrixAndND)
{
    const auto reg = stdReg();
    {  // 2-D complex param + A(i,j) read -> complex scalar
        numkit::ASTNodePtr     root;
        const numkit::ASTNode *fn  = findFunc("function y = f(A)\n  y = A(1,2);\nend\n", root);
        const InferredType     mat = InferredType::concrete(ValueType::COMPLEX, Shape::dims(2, 3));
        const std::string      s   = emitFunction(*fn, {{"A", mat}}, reg).source;
        EXPECT_TRUE(contains(s, "const std::complex<double>* A"));
        EXPECT_TRUE(contains(s, "nk_rt::index2(A,"));
    }
    {  // N-D complex param + A(i,j,k) read -> complex scalar
        numkit::ASTNodePtr     root;
        const numkit::ASTNode *fn = findFunc("function y = f(A)\n  y = A(1,1,1);\nend\n", root);
        const InferredType     nd =
            InferredType::concrete(ValueType::COMPLEX, numkit::codegen::Shape::ndShape({2, 2, 2}));
        const std::string s = emitFunction(*fn, {{"A", nd}}, reg).source;
        EXPECT_TRUE(contains(s, "const std::complex<double>* A"));
        EXPECT_TRUE(contains(s, "nk_rt::indexN(A,"));
    }
}

// Transpose: y = x.' (transpose) / y = x' (ctranspose). A 1-D vector flips
// orientation with an element-for-element copy; ctranspose conjugates a complex
// operand. A scalar transpose is identity (ctranspose conjugates). 2-D/N-D
// transpose is refused (an explicit lowering boundary).
TEST(EmitterFn, VectorTranspose)
{
    const auto reg = stdReg();
    {  // real .' : plain copy, no conjugation
        numkit::ASTNodePtr     root;
        const numkit::ASTNode *fn = findFunc("function y = f(x)\n  y = x.';\nend\n", root);
        ASSERT_NE(fn, nullptr);
        const std::string s = emitFunction(*fn, {{"x", kDoubleRow}}, reg).source;
        EXPECT_TRUE(contains(s, "y[_nk_i] = x[_nk_i];"));
    }
    {  // complex ' : conjugating copy
        numkit::ASTNodePtr     root;
        const numkit::ASTNode *fn = findFunc("function y = f(x)\n  y = x';\nend\n", root);
        const InferredType     cxrow =
            InferredType::concrete(ValueType::COMPLEX, Shape::rowVector());
        const std::string s = emitFunction(*fn, {{"x", cxrow}}, reg).source;
        EXPECT_TRUE(contains(s, "y[_nk_i] = std::conj(x[_nk_i]);"));
    }
    {  // complex .' : NON-conjugating copy (plain transpose)
        numkit::ASTNodePtr     root;
        const numkit::ASTNode *fn = findFunc("function y = f(x)\n  y = x.';\nend\n", root);
        const InferredType     cxrow =
            InferredType::concrete(ValueType::COMPLEX, Shape::rowVector());
        const std::string s = emitFunction(*fn, {{"x", cxrow}}, reg).source;
        EXPECT_TRUE(contains(s, "y[_nk_i] = x[_nk_i];"));
    }
    {  // scalar ctranspose of a complex scalar -> std::conj, no loop
        numkit::ASTNodePtr     root;
        const numkit::ASTNode *fn = findFunc("function y = f(x)\n  y = x';\nend\n", root);
        const std::string s = emitFunction(*fn, {{"x", InferredType::scalar(ValueType::COMPLEX)}}, reg).source;
        EXPECT_TRUE(contains(s, "std::conj(x)"));
    }
    {  // N-D transpose is refused (undefined in MATLAB; explicit boundary)
        numkit::ASTNodePtr     root;
        const numkit::ASTNode *fn = findFunc("function y = f(A)\n  y = A';\nend\n", root);
        const InferredType     nd =
            InferredType::concrete(ValueType::DOUBLE, numkit::codegen::Shape::ndShape({2, 2, 2}));
        EXPECT_THROW(emitFunction(*fn, {{"A", nd}}, reg), std::runtime_error);
    }
}

// 2-D matrix transpose: y = A' / A.' swaps the dims, column-major. y is n x m,
// A is m x n; y(p,q) = A(q,p). ctranspose conjugates a complex matrix.
TEST(EmitterFn, MatrixTranspose)
{
    const auto reg = stdReg();
    {  // real matrix .' : column-major index swap, no conjugation
        numkit::ASTNodePtr     root;
        const numkit::ASTNode *fn  = findFunc("function y = f(A)\n  y = A.';\nend\n", root);
        const InferredType     mat = InferredType::concrete(ValueType::DOUBLE, Shape::dims(2, 3));
        const std::string      s   = emitFunction(*fn, {{"A", mat}}, reg).source;
        // y is 3x2 (its rows companion = 3); reads A column-major with A's rows = 2.
        EXPECT_TRUE(contains(s, "[_nk_i + _nk_j * _nk_y_rows] = A[_nk_j + _nk_i * _nk_A_rows];"));
    }
    {  // complex matrix ' : conjugating transpose
        numkit::ASTNodePtr     root;
        const numkit::ASTNode *fn  = findFunc("function y = f(A)\n  y = A';\nend\n", root);
        const InferredType     mat = InferredType::concrete(ValueType::COMPLEX, Shape::dims(2, 3));
        const std::string      s   = emitFunction(*fn, {{"A", mat}}, reg).source;
        EXPECT_TRUE(contains(s, "= std::conj(A[_nk_j + _nk_i * _nk_A_rows]);"));
    }
}

// Binary math: atan2/hypot are total on R^2 and lower to std:: (scalar args).
TEST(EmitterFn, BinaryMathLowersToStd)
{
    const auto             reg = stdReg();
    numkit::ASTNodePtr     root;
    const numkit::ASTNode *fn =
        findFunc("function r = f(y, x)\n  r = atan2(y, x);\nend\n", root);
    ASSERT_NE(fn, nullptr);
    const EmittedFunction out =
        emitFunction(*fn, {{"y", kDoubleScalar}, {"x", kDoubleScalar}}, reg);
    EXPECT_TRUE(contains(out.source, "std::atan2("));
    EXPECT_FALSE(contains(out.source, "nk_codegen_rt.h"));  // self-contained
}

// ---- array locals ----------------------------------------------------------
// A buffer-typed LOCAL is an owned std::vector: zeros -> .assign, element
// read/write through .data()/.size(). Self-contained (no bridge).
TEST(EmitterFn, ArrayLocalOwnedVector)
{
    const auto             reg = stdReg();
    numkit::ASTNodePtr     root;
    const numkit::ASTNode *fn = findFunc(
        "function y = f(x)\n  n = numel(x);\n  z = zeros(1, n);\n"
        "  for k = 1:n\n    z(k) = x(k) * 2;\n  end\n"
        "  y = zeros(1, n);\n  for k = 1:n\n    y(k) = z(k) + 1;\n  end\nend\n",
        root);
    ASSERT_NE(fn, nullptr);
    const std::string s = emitFunction(*fn, {{"x", kDoubleRow}}, reg).source;
    EXPECT_TRUE(contains(s, "std::vector<double> z;"));  // hoisted owned vector
    EXPECT_TRUE(contains(s, "z.assign("));               // zeros -> assign(numel, 0.0)
    EXPECT_TRUE(contains(s, "z.data()"));                // element access via data()/size()
}

// Elementwise array ARITHMETIC: y = x .* 2 + 1 -> a per-element fill loop
// (whole arrays index at _nk_i). Self-contained, no bridge.
TEST(EmitterFn, ElementwiseArrayArithmetic)
{
    const auto             reg = stdReg();
    numkit::ASTNodePtr     root;
    const numkit::ASTNode *fn = findFunc("function y = f(x)\n  y = x .* 2 + 1;\nend\n", root);
    ASSERT_NE(fn, nullptr);
    const std::string s = emitFunction(*fn, {{"x", kDoubleRow}}, reg).source;
    EXPECT_TRUE(contains(s, "x[_nk_i]"));    // whole array -> current element
    EXPECT_TRUE(contains(s, "[_nk_i] = "));  // elementwise fill loop store
    EXPECT_FALSE(contains(s, "nk_codegen_rt.h"));  // self-contained, no bridge
}

// Native elementwise array MATH: y = sin(x) lowers to a std::sin per-element
// loop — no bridge, self-contained. (sin/cos/erf/… are all elementwise.)
TEST(EmitterFn, ElementwiseArrayMathNative)
{
    const auto             reg = stdReg();
    numkit::ASTNodePtr     root;
    const numkit::ASTNode *fn = findFunc("function y = f(x)\n  y = sin(x);\nend\n", root);
    ASSERT_NE(fn, nullptr);
    const std::string s = emitFunction(*fn, {{"x", kDoubleRow}}, reg).source;  // no bridge
    EXPECT_TRUE(contains(s, "std::sin(x[_nk_i])"));   // per-element native call
    EXPECT_FALSE(contains(s, "nk_codegen_rt.h"));   // self-contained, no runtime
}

// A matrix op (mtimes `*`, not elementwise `.*`) is NOT lowered as elementwise
// — without bridging it falls to the boundary (throws), never silently wrong.
TEST(EmitterFn, MatrixMtimesNotElementwise)
{
    const auto             reg = stdReg();
    numkit::ASTNodePtr     root;
    const numkit::ASTNode *fn = findFunc("function y = f(x)\n  y = x * x;\nend\n", root);
    ASSERT_NE(fn, nullptr);
    EXPECT_THROW(emitFunction(*fn, {{"x", kDoubleRow}}, reg), std::runtime_error);
}

// `*` / `/` with a scalar operand are elementwise SCALING (s*X == s.*X,
// X/s == X./s), lowered as a per-element loop — not a matrix product. Only the
// scalar-scaling cases qualify; matrix*matrix (above) still falls to the
// boundary.
TEST(EmitterFn, ScalarScalingViaMtimesMrdivide)
{
    const auto reg = stdReg();
    {  // 2 * v  (scalar on the left)
        numkit::ASTNodePtr     root;
        const numkit::ASTNode *fn = findFunc("function y = f(v)\n  y = 2 * v;\nend\n", root);
        ASSERT_NE(fn, nullptr);
        const std::string s = emitFunction(*fn, {{"v", kDoubleRow}}, reg).source;
        EXPECT_TRUE(contains(s, "(2.0 * v[_nk_i])"));
        EXPECT_FALSE(contains(s, "nk_codegen_rt.h"));  // self-contained
    }
    {  // v * 2  (scalar on the right)
        numkit::ASTNodePtr     root;
        const numkit::ASTNode *fn = findFunc("function y = f(v)\n  y = v * 2;\nend\n", root);
        const std::string s = emitFunction(*fn, {{"v", kDoubleRow}}, reg).source;
        EXPECT_TRUE(contains(s, "(v[_nk_i] * 2.0)"));
    }
    {  // v / 2  (denominator scalar)
        numkit::ASTNodePtr     root;
        const numkit::ASTNode *fn = findFunc("function y = f(v)\n  y = v / 2;\nend\n", root);
        const std::string s = emitFunction(*fn, {{"v", kDoubleRow}}, reg).source;
        EXPECT_TRUE(contains(s, "(v[_nk_i] / 2.0)"));
    }
}

// 2-D matrix WRITE to a mutable local: A = zeros(3,3); A(i,j) = v. Compile-time
// dims (KnownDims), flat owned vector, column-major index2_set. Self-contained.
TEST(EmitterFn, Matrix2DLocalWrite)
{
    const auto             reg = stdReg();
    numkit::ASTNodePtr     root;
    const numkit::ASTNode *fn = findFunc(
        "function s = f()\n  A = zeros(3, 3);\n  A(1,1) = 5;\n  A(2,2) = 7;\n"
        "  s = A(1,1) + A(2,2);\nend\n",
        root);
    ASSERT_NE(fn, nullptr);
    const std::string s = emitFunction(*fn, {}, reg).source;
    EXPECT_TRUE(contains(s, "std::vector<double> A;"));             // flat 2-D local storage
    EXPECT_TRUE(contains(s, "nk_rt::index2_set(A.data(), 3, 3"));   // column-major write
    EXPECT_TRUE(contains(s, "nk_rt::index2(A.data(), 3, 3"));       // column-major read
}

// N-D (rank>=3) matrix local: A = zeros(2,2,2); A(i,j,k) read/write. Flat owned
// vector + compile-time dims, column-major nk_rt::indexN / indexN_set.
TEST(EmitterFn, NDArrayLocalWrite)
{
    const auto             reg = stdReg();
    numkit::ASTNodePtr     root;
    const numkit::ASTNode *fn = findFunc(
        "function s = f()\n  A = zeros(2, 2, 2);\n  A(1,1,1) = 5;\n  A(2,2,2) = 9;\n"
        "  s = A(1,1,1) + A(2,2,2);\nend\n",
        root);
    ASSERT_NE(fn, nullptr);
    const std::string s = emitFunction(*fn, {}, reg).source;
    EXPECT_TRUE(contains(s, "std::vector<double> A;"));               // flat N-D storage
    EXPECT_TRUE(contains(s, "nk_rt::indexN_set(A.data(), {2, 2, 2}"));  // column-major write
    EXPECT_TRUE(contains(s, "nk_rt::indexN(A.data(), {2, 2, 2}"));      // column-major read
}

// N-D PARAM: pointer + one size_t companion per dim; read-only, column-major.
TEST(EmitterFn, NDParamReadOnly)
{
    const auto             reg = stdReg();
    numkit::ASTNodePtr     root;
    const numkit::ASTNode *fn =
        findFunc("function s = f(A)\n  s = A(1,1,1) + size(A,2);\nend\n", root);
    ASSERT_NE(fn, nullptr);
    const std::string s = emitFunction(
        *fn, {{"A", InferredType::concrete(ValueType::DOUBLE, numkit::codegen::Shape::ndShape({2, 2, 2}))}},
        reg).source;
    EXPECT_TRUE(contains(s, "const double* A, std::size_t _nk_A_d0, std::size_t _nk_A_d1, std::size_t _nk_A_d2"));
    EXPECT_TRUE(contains(s, "nk_rt::indexN(A, {_nk_A_d0, _nk_A_d1, _nk_A_d2}"));  // companions as dims
    EXPECT_TRUE(contains(s, "static_cast<double>(_nk_A_d1)"));            // size(A,2) -> companion
}

// Writing an N-D param (const T*) is refused — the explicit boundary.
TEST(EmitterFn, NDParamWriteRefused)
{
    const auto             reg = stdReg();
    numkit::ASTNodePtr     root;
    const numkit::ASTNode *fn =
        findFunc("function s = f(A)\n  A(1,1,1) = 5;\n  s = A(1,1,1);\nend\n", root);
    ASSERT_NE(fn, nullptr);
    EXPECT_THROW(emitFunction(*fn,
                              {{"A", InferredType::concrete(ValueType::DOUBLE,
                                                            numkit::codegen::Shape::ndShape({2, 2, 2}))}},
                              reg),
                 std::runtime_error);
}

// N-D shape queries: size(A,k) (literal k) / ndims(A) / numel(A) on an N-D
// array fold to compile-time constants.
TEST(EmitterFn, NDShapeQueries)
{
    const auto             reg = stdReg();
    numkit::ASTNodePtr     root;
    const numkit::ASTNode *fn = findFunc(
        "function s = f()\n  A = zeros(2, 3, 4);\n"
        "  s = size(A,1) + size(A,3) + numel(A) + ndims(A);\nend\n",
        root);
    ASSERT_NE(fn, nullptr);
    const std::string s = emitFunction(*fn, {}, reg).source;
    EXPECT_TRUE(contains(s, "static_cast<double>(2)"));          // size(A,1)
    EXPECT_TRUE(contains(s, "static_cast<double>(4)"));          // size(A,3)
    EXPECT_TRUE(contains(s, "static_cast<double>(2 * 3 * 4)"));  // numel = product of dims
    EXPECT_TRUE(contains(s, "static_cast<double>(3)"));          // ndims = rank
}

// N-D OUTPUT: a function returning a rank>=3 array -> a caller-allocated
// out-param (a MUTABLE pointer + one size_t companion per dim, passed IN).
// The body zero-fills over the companion product then writes via indexN_set
// with the COMPANION dims (runtime values) — so a runtime-dim N-D output
// works too. Mirror of the N-D param ABI, but mutable + void return.
TEST(EmitterFn, NDOutputAsOutParam)
{
    const auto             reg = stdReg();
    numkit::ASTNodePtr     root;
    const numkit::ASTNode *fn = findFunc(
        "function y = f(a)\n  y = zeros(2, 2, 2);\n  y(1,1,1) = a;\n  y(2,2,2) = a * 2;\nend\n",
        root);
    ASSERT_NE(fn, nullptr);
    const std::string s =
        emitFunction(*fn, {{"a", InferredType::scalar(ValueType::DOUBLE)}}, reg).source;
    EXPECT_TRUE(contains(s, "void f(double a, double* __restrict y, "
                            "std::size_t _nk_y_d0, std::size_t _nk_y_d1, std::size_t _nk_y_d2"));  // mutable + void
    EXPECT_TRUE(contains(s, "_nk_i < (_nk_y_d0 * _nk_y_d1 * _nk_y_d2)"));               // zeros() fills the product
    EXPECT_TRUE(contains(s, "nk_rt::indexN_set(y, {_nk_y_d0, _nk_y_d1, _nk_y_d2}"));  // companion dims, mutable ptr
}

// 2-D matrix OUTPUT: a function returning a matrix -> caller-allocated out-param
// (mutable ptr + rows/cols companions, column-major). Mirror of the N-D-output
// brick on the 2-D fast path (index2_set). Completes the array-output story
// (scalar / 1-D / 2-D / N-D all return).
TEST(EmitterFn, Matrix2DOutputAsOutParam)
{
    const auto             reg = stdReg();
    numkit::ASTNodePtr     root;
    const numkit::ASTNode *fn = findFunc(
        "function M = f(a)\n  M = zeros(2, 3);\n  M(1,1) = a;\n  M(2,3) = a * 2;\nend\n", root);
    ASSERT_NE(fn, nullptr);
    const std::string s = emitFunction(*fn, {{"a", kDoubleScalar}}, reg).source;
    EXPECT_TRUE(contains(s, "void f(double a, double* __restrict M, "
                            "std::size_t _nk_M_rows, std::size_t _nk_M_cols"));  // mutable + void
    EXPECT_TRUE(contains(s, "_nk_i < (_nk_M_rows * _nk_M_cols)"));               // zeros() fill bound
    EXPECT_TRUE(contains(s, "nk_rt::index2_set(M, _nk_M_rows, _nk_M_cols"));     // column-major write
}

// 1-D size(vec, dim): orientation (row vs col) is recorded from the compile-time
// type and folds size(vec,1)/size(vec,2) to constants — though the RawBuffer ABI
// (ptr+len) erases orientation. Row: size(.,1)=1, size(.,2)=len; col: swapped.
TEST(EmitterFn, OneDSizeOrientation)
{
    const auto             reg = stdReg();
    const char            *src = "function s = f(x)\n  s = size(x,1)*100 + size(x,2);\nend\n";
    numkit::ASTNodePtr     root;
    const numkit::ASTNode *fn = findFunc(src, root);
    ASSERT_NE(fn, nullptr);
    const std::string row = emitFunction(*fn, {{"x", kDoubleRow}}, reg).source;  // 1 x len
    EXPECT_TRUE(contains(row, "(static_cast<double>(1) * 100.0) + static_cast<double>(_nk_x_len)"));

    numkit::ASTNodePtr     root2;
    const numkit::ASTNode *fn2 = findFunc(src, root2);
    const InferredType     colv =
        InferredType::concrete(ValueType::DOUBLE, numkit::codegen::Shape::colVector());
    const std::string col = emitFunction(*fn2, {{"x", colv}}, reg).source;       // len x 1
    EXPECT_TRUE(contains(col, "(static_cast<double>(_nk_x_len) * 100.0) + static_cast<double>(1)"));
}

// `s = size(A)` (no dim) -> a 1 x rank row filled with A's per-axis sizes,
// native (no bridge). 1-D row: [1, len]; 2-D: [rows, cols]; N-D: [d0, d1, ...].
TEST(EmitterFn, SizeNoDimRowVector)
{
    const auto reg = stdReg();
    {  // 1-D row operand -> [1, len]   (s is the output: written directly)
        numkit::ASTNodePtr     root;
        const numkit::ASTNode *fn = findFunc("function s = f(x)\n  s = size(x);\nend\n", root);
        ASSERT_NE(fn, nullptr);
        const std::string s = emitFunction(*fn, {{"x", kDoubleRow}}, reg).source;
        EXPECT_TRUE(contains(s, "s[0] = static_cast<double>(1);"));
        EXPECT_TRUE(contains(s, "s[1] = static_cast<double>(_nk_x_len);"));
    }
    {  // 2-D operand -> [rows, cols]
        numkit::ASTNodePtr     root;
        const numkit::ASTNode *fn = findFunc("function s = f(A)\n  s = size(A);\nend\n", root);
        const InferredType     mat = InferredType::concrete(ValueType::DOUBLE, Shape::dims(3, 4));
        const std::string      s   = emitFunction(*fn, {{"A", mat}}, reg).source;
        EXPECT_TRUE(contains(s, "s[0] = static_cast<double>(_nk_A_rows);"));
        EXPECT_TRUE(contains(s, "s[1] = static_cast<double>(_nk_A_cols);"));
    }
    {  // N-D operand -> [d0, d1, d2]
        numkit::ASTNodePtr     root;
        const numkit::ASTNode *fn = findFunc("function s = f(A)\n  s = size(A);\nend\n", root);
        const InferredType     nd =
            InferredType::concrete(ValueType::DOUBLE, numkit::codegen::Shape::ndShape({2, 3, 4}));
        const std::string s = emitFunction(*fn, {{"A", nd}}, reg).source;
        EXPECT_TRUE(contains(s, "s[2] = static_cast<double>(_nk_A_d2);"));  // 3rd axis
    }
}

// [r, c] = size(A): native two-output size. r = axis 0 (rows); c folds the
// remaining axes (cols for a matrix, the trailing-dim product for N-D). Both
// targets are real scalars -- no array producer.
TEST(EmitterFn, SizeTwoOutputRowsCols)
{
    const auto reg = stdReg();
    {  // 2-D operand -> r = rows, c = cols (both scalar OUTPUT params)
        numkit::ASTNodePtr     root;
        const numkit::ASTNode *fn =
            findFunc("function [r, c] = f(A)\n  [r, c] = size(A);\nend\n", root);
        ASSERT_NE(fn, nullptr);
        const InferredType mat = InferredType::concrete(ValueType::DOUBLE, Shape::dims(3, 4));
        const std::string  s   = emitFunction(*fn, {{"A", mat}}, reg).source;
        EXPECT_TRUE(contains(s, "r = static_cast<double>(_nk_A_rows);"));
        EXPECT_TRUE(contains(s, "c = static_cast<double>(_nk_A_cols);"));
    }
    {  // N-D operand -> c folds the trailing dims (d1 * d2), matching MATLAB
        numkit::ASTNodePtr     root;
        const numkit::ASTNode *fn =
            findFunc("function [r, c] = f(A)\n  [r, c] = size(A);\nend\n", root);
        const InferredType nd =
            InferredType::concrete(ValueType::DOUBLE, numkit::codegen::Shape::ndShape({2, 3, 4}));
        const std::string s = emitFunction(*fn, {{"A", nd}}, reg).source;
        EXPECT_TRUE(contains(s, "r = static_cast<double>(_nk_A_d0);"));
        EXPECT_TRUE(contains(s, "c = static_cast<double>(_nk_A_d1 * _nk_A_d2);"));
    }
    {  // 1-D row operand -> r = 1, c = len; r,c are scalar LOCALS (must hoist)
        numkit::ASTNodePtr     root;
        const numkit::ASTNode *fn = findFunc(
            "function y = f(x)\n  [r, c] = size(x);\n  y = r * 100 + c;\nend\n", root);
        const std::string s = emitFunction(*fn, {{"x", kDoubleRow}}, reg).source;
        EXPECT_TRUE(contains(s, "r = static_cast<double>(1);"));
        EXPECT_TRUE(contains(s, "c = static_cast<double>(_nk_x_len);"));
    }
}

// RUNTIME-dim N-D LOCAL: zeros(m,n,p) with variable dims -> per-dim size_t
// companion vars (hoisted, set from the args), a flat owned vector sized to
// their product; indexN / size / numel read the vars (ndims still folds to the
// literal rank). Closes "full N-D": N-D now lives in every position, const
// AND runtime dims.
TEST(EmitterFn, NDRuntimeLocal)
{
    const auto             reg = stdReg();
    numkit::ASTNodePtr     root;
    const numkit::ASTNode *fn = findFunc(
        "function s = f(m, n, p)\n  A = zeros(m, n, p);\n  A(1,1,1) = 7;\n"
        "  s = A(1,1,1) + size(A,2) + numel(A);\nend\n",
        root);
    ASSERT_NE(fn, nullptr);
    const std::vector<ParamSpec> params = {{"m", InferredType::scalar(ValueType::DOUBLE)},
                                           {"n", InferredType::scalar(ValueType::DOUBLE)},
                                           {"p", InferredType::scalar(ValueType::DOUBLE)}};
    const std::string s = emitFunction(*fn, params, reg).source;
    EXPECT_TRUE(contains(s, "std::size_t _nk_A_d0 = 0, _nk_A_d1 = 0, _nk_A_d2 = 0;"));        // hoisted vars
    EXPECT_TRUE(contains(s, "_nk_A_d0 = nk_rt::dim(m);"));                            // captured (guarded)
    EXPECT_TRUE(contains(s, "A.assign(_nk_A_d0 * _nk_A_d1 * _nk_A_d2, 0.0);"));               // sized to product
    EXPECT_TRUE(contains(s, "nk_rt::indexN_set(A.data(), {_nk_A_d0, _nk_A_d1, _nk_A_d2}"));   // runtime dims
    EXPECT_TRUE(contains(s, "static_cast<double>(_nk_A_d1)"));                        // size(A,2) -> var
    EXPECT_TRUE(contains(s, "static_cast<double>(_nk_A_d0 * _nk_A_d1 * _nk_A_d2)"));          // numel -> product
}

// N-D is REFUSED by the 1-D-only elementwise path (explicit boundary, not
// broken C++). `y = A .* 2` with A 3-D: the flat per-element loop bounds on a
// lenVar an N-D param doesn't have, and numel-matching is unsound for N-D, so
// the emitter throws rather than emit malformed/wrong code (Contract 2).
TEST(EmitterFn, ElementwiseOnNDRefused)
{
    const auto             reg = stdReg();
    numkit::ASTNodePtr     root;
    const numkit::ASTNode *fn = findFunc("function y = f(A)\n  y = A .* 2;\nend\n", root);
    ASSERT_NE(fn, nullptr);
    EXPECT_THROW(emitFunction(*fn,
                              {{"A", InferredType::concrete(ValueType::DOUBLE,
                                                            numkit::codegen::Shape::ndShape({2, 2, 2}))}},
                              reg),
                 std::runtime_error);
}

// Reserved-companion coexistence: a user var named `x_len` no longer clashes
// with the length companion of array param `x`. The companion is `_nk_x_len`
// (the `_nk_` prefix lives in the underscore namespace a MATLAB identifier can
// never enter), so both names coexist in the emitted C++ — this valid MATLAB
// that the emitter once had to refuse now compiles. The collision is
// structurally impossible, so there is no check to test.
TEST(EmitterFn, CompanionDoesNotClashWithUserVar)
{
    const auto             reg = stdReg();
    numkit::ASTNodePtr     root;
    const numkit::ASTNode *fn =
        findFunc("function y = f(x)\n  x_len = 3;\n  y = x(x_len);\nend\n", root);
    ASSERT_NE(fn, nullptr);
    const std::string s =
        emitFunction(*fn, {{"x", InferredType::concrete(ValueType::DOUBLE, Shape::rowVector())}}, reg)
            .source;
    EXPECT_TRUE(contains(s, "std::size_t _nk_x_len"));  // length companion (prefixed)
    EXPECT_TRUE(contains(s, "double x_len"));           // the user var — coexists, no clash
}

// A user identifier containing "__" is emitted verbatim, and "__" anywhere is
// reserved to the implementation ([lex.name]) — UB. The emitter refuses it with
// a clear message (the last no-UB hole: synthesised names escape "__", user
// names stay verbatim, so a "__" user name must be rejected).
TEST(EmitterFn, DoubleUnderscoreUserNameRefused)
{
    const auto             reg = stdReg();
    numkit::ASTNodePtr     root;
    const numkit::ASTNode *fn =
        findFunc("function y = f(x)\n  a__b = x * 2;\n  y = a__b + 1;\nend\n", root);
    ASSERT_NE(fn, nullptr);
    EXPECT_THROW(emitFunction(*fn, {{"x", kDoubleScalar}}, reg), std::runtime_error);
}

// A single or trailing underscore is NOT reserved — `x_` emits verbatim and its
// companion escapes the trailing '_' (`_nk_x_0_len`), so it compiles fine.
TEST(EmitterFn, SingleUnderscoreUserNameAllowed)
{
    const auto             reg = stdReg();
    numkit::ASTNodePtr     root;
    const numkit::ASTNode *fn = findFunc("function s = g(x_)\n  s = numel(x_);\nend\n", root);
    ASSERT_NE(fn, nullptr);
    const std::string s = emitFunction(*fn, {{"x_", kDoubleRow}}, reg).source;
    EXPECT_TRUE(contains(s, "const double* x_"));  // verbatim, legal (no "__")
    EXPECT_TRUE(contains(s, "_nk_x_0_len"));        // companion escapes the trailing '_'
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
              "void biquad(const double* x, std::size_t _nk_x_len, double b0, "
              "double b1, double b2, double a1, double a2, double* __restrict y, "
              "std::size_t _nk_y_len)");

    // hoisted scalar locals (k is the promoted counter -> declared in the
    // for, not hoisted; see brick 6 below)
    for (const char *v : {"n", "x1", "x2", "xn", "y1", "y2", "yn"})
        EXPECT_TRUE(contains(s, std::string("double ") + v + " = 0.0;")) << v;

    EXPECT_TRUE(contains(s, "n = static_cast<double>(_nk_x_len);"));        // numel(x)
    EXPECT_TRUE(contains(s, "for (std::size_t _nk_i = 0; _nk_i < _nk_y_len; ++_nk_i)"));  // y = zeros(1,n)
    EXPECT_TRUE(contains(s, "y[_nk_i] = 0.0;"));
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

    EXPECT_TRUE(contains(s, "for (std::size_t k = 0; k < _nk_x_len; ++k)"));
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
    EXPECT_TRUE(contains(s, "nk_rt::index(x, _nk_x_len, k)"));        // checked read
    EXPECT_TRUE(contains(s, "nk_rt::index_set(y, _nk_y_len, k,"));    // checked write
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
    EXPECT_TRUE(contains(s, "nk_rt::index_set(y, _nk_y_len, k,"));
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

    EXPECT_TRUE(contains(s, "const double* A, std::size_t _nk_A_rows, std::size_t _nk_A_cols"));
    EXPECT_TRUE(contains(s, "nk_rt::index2(A, _nk_A_rows, _nk_A_cols, 1.0, 1.0)"));
    EXPECT_TRUE(contains(s, "nk_rt::index2(A, _nk_A_rows, _nk_A_cols, 3.0, 2.0)"));
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
    EXPECT_TRUE(contains(s, "void two_1d(double x, double& a, double& b)"));
    EXPECT_TRUE(contains(s, "two_1d(x, p, q);"));   // targets appended as out-args
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

    EXPECT_EQ(out.name, "run_1o0");
    EXPECT_TRUE(contains(s, "struct Rect {"));
    EXPECT_TRUE(contains(s, "double Rect_0_0area_1o0(Rect obj)"));  // method specialisation
    EXPECT_TRUE(contains(s, "a = (obj.w * obj.h);"));            // field reads in the body
    EXPECT_TRUE(contains(s, "Rect_0_0area_1o0(p)"));               // call site (self = p)
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

    EXPECT_EQ(out.name, "f_1d");                              // mangled entry symbol
    EXPECT_TRUE(contains(s, "double f_1d(double x);"));       // forward decls
    EXPECT_TRUE(contains(s, "double g_1d(double x);"));
    EXPECT_TRUE(contains(s, "double f_1d(double x) {"));      // definitions
    EXPECT_TRUE(contains(s, "double g_1d(double x) {"));
    EXPECT_TRUE(contains(s, "g_1d(x)"));                      // f calls the specialisation
    EXPECT_TRUE(contains(s, "y = (g_1d(x) + 1.0);"));
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
    EXPECT_TRUE(contains(out.source, "g_1dr(v, _nk_v_len)"));            // ptr + len at the call site
    EXPECT_TRUE(contains(out.source, "double g_1dr(const double* v, std::size_t _nk_v_len)"));
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