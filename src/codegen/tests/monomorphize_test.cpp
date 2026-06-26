// codegen/tests/monomorphize_test.cpp
//
// Engine brick 1a: the monomorphizing interprocedural inference. A user
// function call types by inferring the callee's body specialised to the
// call's argument types; this plugs into the TransferRegistry so inferExpr
// routes user calls exactly like builtins. Recursion breaks to Dynamic
// (sound). Lattice-level (no emitter / compiler yet).

#include <numkit/codegen/monomorphize.hpp>
#include <numkit/codegen/transfer.hpp>

#include <numkit/core/lexer.hpp>
#include <numkit/core/parser.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <string>

using numkit::ValueType;
using namespace numkit::codegen;

namespace {

// A parsed program with its function table + a registry carrying the
// standard transfers AND the user functions. Held together because the
// registry's user-function transfers borrow the AST and the registry.
struct Prog {
    numkit::ASTNodePtr root;
    FunctionTable      table;
    TransferRegistry   reg;
};

std::shared_ptr<Prog> build(const std::string &src)
{
    auto           p = std::make_shared<Prog>();
    numkit::Lexer  lex(src);
    numkit::Parser parser(lex.tokenize());
    p->root = parser.parse();
    collectFunctions(*p->root, p->table);
    registerStandardTransfers(p->reg);
    registerUserFunctions(p->reg, p->table);
    return p;
}

ArgInfo dbl()
{
    return ArgInfo(InferredType::scalar(ValueType::DOUBLE), ConstVal::unknown());
}
ArgInfo row()
{
    return ArgInfo(InferredType::concrete(ValueType::DOUBLE, Shape::rowVector()),
                   ConstVal::unknown());
}

}  // namespace

TEST(Monomorphize, CollectsFunctions)
{
    auto p = build("function y = f(x)\n  y = x;\nend\n"
                   "function y = g(x)\n  y = x;\nend\n");
    EXPECT_EQ(p->table.size(), 2u);
    EXPECT_TRUE(p->table.has("f"));
    EXPECT_TRUE(p->table.has("g"));
}

TEST(Monomorphize, SimpleReturnType)
{
    auto p = build("function y = g(x)\n  y = x*2;\nend\n");
    ASSERT_TRUE(p->table.has("g"));
    const InferredType r = inferFunctionReturn(*p->table.find("g"), {dbl()}, p->reg);
    EXPECT_TRUE(r.isUnboxableScalar());
    EXPECT_EQ(r.dtype, ValueType::DOUBLE);
}

// inferExpr routes a user call through reg.apply, just like a builtin.
TEST(Monomorphize, RoutesThroughRegistry)
{
    auto               p = build("function y = g(x)\n  y = x*2;\nend\n");
    const InferredType r = p->reg.apply("g", {dbl()});
    EXPECT_TRUE(r.isUnboxableScalar());
    EXPECT_EQ(r.dtype, ValueType::DOUBLE);
}

// Recursion (Bottom-fixpoint foundation, Rec.1): Bottom is ABSORBING in a
// transfer. A ⊥ operand -- the seed of an unresolved recursive self-call --
// propagates to ⊥, NOT Dynamic, so `n * ⊥` stays ⊥ and the return-type fixpoint
// can converge (the join with the base case collapses ⊥ to the real type).
// reg.apply / applyMulti short-circuit on any ⊥ arg, before the per-fn transfer.
TEST(Monomorphize, BottomIsAbsorbingInTransfers)
{
    auto          p = build("function y = f(x)\n  y = x;\nend\n");
    const ArgInfo bot(InferredType::bottom(), ConstVal::unknown());
    // sanity: a normal binary op still types concretely (no ⊥ arg).
    EXPECT_TRUE(p->reg.apply("plus", {dbl(), dbl()}).isConcrete());
    // a ⊥ operand makes the result ⊥, in either position, builtin or user fn.
    EXPECT_TRUE(p->reg.apply("plus", {bot, dbl()}).isBottom());
    EXPECT_TRUE(p->reg.apply("times", {dbl(), bot}).isBottom());
    EXPECT_TRUE(p->reg.apply("f", {bot}).isBottom());
    // multi-output: a ⊥ arg yields "no info" (empty -> the caller defaults Dynamic).
    EXPECT_TRUE(p->reg.applyMulti("f", {bot}).empty());
}

// Rec.2: DIRECT self-recursion now infers a CONCRETE return type via the Bottom-
// fixpoint (it used to break to Dynamic). factorial: ⊥ seed -> else y = n * ⊥ = ⊥,
// join(DOUBLE base-case, ⊥) = DOUBLE -> fixpoint DOUBLE (converges in 2 iters).
TEST(Monomorphize, SelfRecursionInfersConcreteViaFixpoint)
{
    auto p = build("function y = fact(n)\n"
                   "  if n <= 1\n    y = 1;\n  else\n    y = n * fact(n - 1);\n  end\n"
                   "end\n");
    ASSERT_TRUE(p->table.has("fact"));
    const InferredType r = p->reg.apply("fact", {dbl()});  // the transfer runs the fixpoint
    EXPECT_TRUE(r.isUnboxableScalar());
    EXPECT_EQ(r.dtype, ValueType::DOUBLE);
}

// fib has TWO same-signature self-calls (fib(n-1) + fib(n-2)); both read the shared
// estimate, and the fixpoint still converges to DOUBLE.
TEST(Monomorphize, FibTwoSelfCallsConvergeToConcrete)
{
    auto p = build("function y = fib(n)\n"
                   "  if n < 2\n    y = n;\n  else\n    y = fib(n - 1) + fib(n - 2);\n  end\n"
                   "end\n");
    const InferredType r = p->reg.apply("fib", {dbl()});
    EXPECT_TRUE(r.isUnboxableScalar());
    EXPECT_EQ(r.dtype, ValueType::DOUBLE);
}

// f calls g — the return type flows through the chain.
TEST(Monomorphize, ChainedCall)
{
    auto p = build("function y = f(x)\n  y = g(x) + 1;\nend\n"
                   "function y = g(x)\n  y = x*2;\nend\n");
    const InferredType r = inferFunctionReturn(*p->table.find("f"), {dbl()}, p->reg);
    EXPECT_TRUE(r.isUnboxableScalar());
    EXPECT_EQ(r.dtype, ValueType::DOUBLE);
}

// Array argument flows through: scalar * rowvector -> rowvector double.
TEST(Monomorphize, ArrayFlowsThrough)
{
    auto p = build("function y = scale(v, s)\n  y = v * s;\nend\n");
    const InferredType r = inferFunctionReturn(*p->table.find("scale"), {row(), dbl()}, p->reg);
    ASSERT_TRUE(r.isConcrete());
    EXPECT_EQ(r.dtype, ValueType::DOUBLE);
    EXPECT_FALSE(r.shape.isScalar());
}

// A base-case-free self-recursion (y = f(x) + 1, no `if`) never returns. Under the
// fixpoint (Rec.2) the self-call seeds ⊥ and the only contribution is f(x)+1 = ⊥+1
// = ⊥ (⊥ absorbing in transfers) with no base-case branch to join a concrete type
// -> the fixpoint converges to ⊥ (Bottom = "no value reaches here"), which is the
// precise truth for a non-terminating function (vs the old coarser Dynamic break).
TEST(Monomorphize, NonTerminatingRecursionInfersBottom)
{
    auto p = build("function y = f(x)\n  y = f(x) + 1;\nend\n");
    const InferredType r = p->reg.apply("f", {dbl()});
    EXPECT_TRUE(r.isBottom());
}

// Wrong argument count -> Dynamic (MVP requires exact arity).
TEST(Monomorphize, ArityMismatchDynamic)
{
    auto p = build("function y = g(x)\n  y = x*2;\nend\n");
    const InferredType r = inferFunctionReturn(*p->table.find("g"), {dbl(), dbl()}, p->reg);
    EXPECT_TRUE(r.isDynamic());
}

// An unknown name (no user fn, no builtin transfer) stays Dynamic.
TEST(Monomorphize, UnknownCallDynamic)
{
    auto p = build("function y = g(x)\n  y = x;\nend\n");
    EXPECT_TRUE(p->reg.apply("nope", {dbl()}).isDynamic());
}

// ── #2b: multi-output ─────────────────────────────────────────────────
TEST(Monomorphize, MultiOutputReturns)
{
    auto p = build("function [a, b] = f(x)\n  a = x + 1;\n  b = x * 2;\nend\n");
    const auto outs = inferFunctionReturns(*p->table.find("f"), {dbl()}, p->reg);
    ASSERT_EQ(outs.size(), 2u);
    EXPECT_TRUE(outs[0].isUnboxableScalar());
    EXPECT_TRUE(outs[1].isUnboxableScalar());
    EXPECT_EQ(p->reg.applyMulti("f", {dbl()}).size(), 2u);     // routed via the registry
    EXPECT_TRUE(p->reg.apply("f", {dbl()}).isDynamic());       // single projection of 2-out -> Dynamic
}

TEST(Monomorphize, MultiAssignTypesTargets)
{
    auto p = build(
        "function y = g(x)\n  [p, q] = f(x);\n  y = p + q;\nend\n"
        "function [a, b] = f(x)\n  a = x;\n  b = x * 2;\nend\n");
    const InferredType y = inferFunctionReturn(*p->table.find("g"), {dbl()}, p->reg);
    EXPECT_TRUE(y.isUnboxableScalar());  // [p,q] typed double -> y = p+q double
    EXPECT_EQ(y.dtype, ValueType::DOUBLE);
}
