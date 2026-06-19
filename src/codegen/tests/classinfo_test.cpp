// codegen/tests/classinfo_test.cpp
//
// Brick 4: the Object lattice tier + the class table. Parse a classdef
// into a ClassInfo (fields with inferred types, methods, value-vs-handle),
// and verify the v1-subset refusals (inheritance, accessors, attribute
// properties, missing default) are loud — never silently miscompiled.

#include <numkit/codegen/classinfo.hpp>
#include <numkit/codegen/inference.hpp>
#include <numkit/codegen/transfer.hpp>
#include <numkit/codegen/type_lattice.hpp>

#include <numkit/core/lexer.hpp>
#include <numkit/core/parser.hpp>

#include <gtest/gtest.h>

using numkit::ValueType;
using namespace numkit::codegen;

namespace {

const numkit::ASTNode *findClass(const std::string &src, numkit::ASTNodePtr &root)
{
    numkit::Lexer  lex(src);
    numkit::Parser parser(lex.tokenize());
    root = parser.parse();
    for (const auto &c : root->children)
        if (c && c->type == numkit::NodeType::CLASSDEF_DEF) return c.get();
    return nullptr;
}

}  // namespace

// ── lattice Object tier ───────────────────────────────────────────────
TEST(ObjectLattice, ObjectTypeBasics)
{
    const InferredType o = InferredType::object(2);
    EXPECT_TRUE(o.isObject());
    EXPECT_TRUE(o.isConcrete());
    EXPECT_EQ(o.classId, 2);
    EXPECT_FALSE(o.isUnboxableScalar());  // an object is not a C++ primitive
}

TEST(ObjectLattice, JoinSameClassKeepsIt)
{
    EXPECT_EQ(join(InferredType::object(1), InferredType::object(1)), InferredType::object(1));
}

TEST(ObjectLattice, JoinDifferentClassesIsDynamic)
{
    EXPECT_TRUE(join(InferredType::object(1), InferredType::object(2)).isDynamic());
    // object vs a numeric is type-unstable -> Dynamic too
    EXPECT_TRUE(join(InferredType::object(1), InferredType::scalar(ValueType::DOUBLE)).isDynamic());
}

TEST(ObjectLattice, NumericUnaffectedByClassId)
{
    // classId defaults to -1 for numerics, so equality / join are unchanged.
    EXPECT_EQ(InferredType::scalar(ValueType::DOUBLE), InferredType::scalar(ValueType::DOUBLE));
    EXPECT_EQ(join(InferredType::scalar(ValueType::DOUBLE), InferredType::scalar(ValueType::DOUBLE)),
              InferredType::scalar(ValueType::DOUBLE));
}

// ── class table ───────────────────────────────────────────────────────
TEST(ClassInfoTest, ParseValueClass)
{
    TransferRegistry reg;
    registerStandardTransfers(reg);
    numkit::ASTNodePtr root;
    const numkit::ASTNode *cd = findClass(
        "classdef Point\n"
        "  properties\n    x = 0\n    y = 0\n  end\n"
        "  methods\n    function d = dist(obj)\n      d = obj.x;\n    end\n  end\n"
        "end\n",
        root);
    ASSERT_NE(cd, nullptr);

    const ClassInfo ci = buildClassInfo(*cd, 0, reg);
    EXPECT_EQ(ci.name, "Point");
    EXPECT_FALSE(ci.isHandle);
    ASSERT_EQ(ci.fields.size(), 2u);
    EXPECT_EQ(ci.fields[0].name, "x");
    EXPECT_EQ(ci.fields[0].type.dtype, ValueType::DOUBLE);
    EXPECT_TRUE(ci.fields[0].type.isUnboxableScalar());
    EXPECT_NE(ci.field("y"), nullptr);
    EXPECT_NE(ci.method("dist"), nullptr);
    EXPECT_EQ(ci.method("nope"), nullptr);
}

TEST(ClassInfoTest, HandleClassDetected)
{
    TransferRegistry reg;
    registerStandardTransfers(reg);
    numkit::ASTNodePtr root;
    const numkit::ASTNode *cd = findClass(
        "classdef Counter < handle\n  properties\n    n = 0\n  end\nend\n", root);
    ASSERT_NE(cd, nullptr);
    const ClassInfo ci = buildClassInfo(*cd, 0, reg);
    EXPECT_TRUE(ci.isHandle);
}

TEST(ClassInfoTest, RegistryAssignsIds)
{
    TransferRegistry reg;
    registerStandardTransfers(reg);
    numkit::ASTNodePtr root;
    const numkit::ASTNode *cd = findClass(
        "classdef A\n  properties\n    v = 0\n  end\nend\n", root);
    ASSERT_NE(cd, nullptr);
    ClassRegistry creg;
    const int     id = creg.add(buildClassInfo(*cd, 0, reg));
    EXPECT_EQ(id, 0);
    EXPECT_EQ(creg.idOf("A"), 0);
    EXPECT_TRUE(creg.has("A"));
    ASSERT_NE(creg.byId(0), nullptr);
    EXPECT_EQ(creg.byId(0)->name, "A");
    EXPECT_EQ(creg.byName("A")->id, 0);
    EXPECT_EQ(creg.idOf("missing"), -1);
}

// ── v1-subset refusals (loud, never miscompiled) ──────────────────────
TEST(ClassInfoTest, RefuseInheritance)
{
    TransferRegistry reg;
    registerStandardTransfers(reg);
    numkit::ASTNodePtr root;
    const numkit::ASTNode *cd =
        findClass("classdef Dog < Animal\n  properties\n    a = 0\n  end\nend\n", root);
    ASSERT_NE(cd, nullptr);
    EXPECT_FALSE(classRefusalReason(*cd).empty());
    EXPECT_THROW(buildClassInfo(*cd, 0, reg), std::runtime_error);
}

TEST(ClassInfoTest, RefuseAccessor)
{
    TransferRegistry reg;
    registerStandardTransfers(reg);
    numkit::ASTNodePtr root;
    const numkit::ASTNode *cd = findClass(
        "classdef P\n  properties\n    x = 0\n  end\n"
        "  methods\n    function v = get.x(obj)\n      v = 1;\n    end\n  end\nend\n",
        root);
    ASSERT_NE(cd, nullptr);
    EXPECT_FALSE(classRefusalReason(*cd).empty());
    EXPECT_THROW(buildClassInfo(*cd, 0, reg), std::runtime_error);
}

TEST(ClassInfoTest, RefusePropertyWithoutDefault)
{
    TransferRegistry reg;
    registerStandardTransfers(reg);
    numkit::ASTNodePtr root;
    const numkit::ASTNode *cd =
        findClass("classdef Q\n  properties\n    x\n  end\nend\n", root);
    ASSERT_NE(cd, nullptr);
    EXPECT_THROW(buildClassInfo(*cd, 0, reg), std::runtime_error);  // no concrete default
}

// ── field-access inference (brick 5b): obj.field types to the field's type
// when a ClassRegistry is supplied; Dynamic otherwise. ─────────────────
TEST(ClassInfoTest, FieldAccessInference)
{
    TransferRegistry reg;
    registerStandardTransfers(reg);
    numkit::ASTNodePtr croot;
    const numkit::ASTNode *cd = findClass(
        "classdef Point\n  properties\n    x = 0\n    y = 0\n  end\nend\n", croot);
    ASSERT_NE(cd, nullptr);
    ClassRegistry creg;
    const int     id = creg.add(buildClassInfo(*cd, 0, reg));

    // a tiny program whose statements hold `p.x` / `p.nope` as RHS exprs
    numkit::Lexer  lex("z = p.x;\nw = p.nope;\n");
    numkit::Parser parser(lex.tokenize());
    auto           prog = parser.parse();
    const numkit::ASTNode &okField  = *prog->children[0]->children[1];  // p.x
    const numkit::ASTNode &badField = *prog->children[1]->children[1];  // p.nope
    ASSERT_EQ(okField.type, numkit::NodeType::FIELD_ACCESS);

    TypeEnv env;
    env.set("p", {InferredType::object(id), ConstVal::unknown()});

    // with the registry: p.x -> double scalar
    const InferredType tx = inferExpr(okField, env, reg, &creg).type;
    EXPECT_TRUE(tx.isUnboxableScalar());
    EXPECT_EQ(tx.dtype, ValueType::DOUBLE);

    // unknown field -> Dynamic; no registry -> Dynamic
    EXPECT_TRUE(inferExpr(badField, env, reg, &creg).type.isDynamic());
    EXPECT_TRUE(inferExpr(okField, env, reg, nullptr).type.isDynamic());
}

// SOUNDNESS: arithmetic / comparison / math on objects must NOT be typed
// numeric (the operator may be overloaded by the class) -> Dynamic.
TEST(ClassInfoTest, ObjectArithmeticIsDynamic)
{
    TransferRegistry reg;
    registerStandardTransfers(reg);
    numkit::ASTNodePtr croot;
    const numkit::ASTNode *cd =
        findClass("classdef Vec\n  properties\n    x = 0\n  end\nend\n", croot);
    ASSERT_NE(cd, nullptr);
    ClassRegistry creg;
    const int     id = creg.add(buildClassInfo(*cd, 0, reg));

    numkit::Lexer  lex("p = a + b;\nq = a == b;\nr = sin(a);\ns = -a;\n");
    numkit::Parser parser(lex.tokenize());
    auto           prog = parser.parse();

    TypeEnv env;
    env.set("a", {InferredType::object(id), ConstVal::unknown()});
    env.set("b", {InferredType::object(id), ConstVal::unknown()});

    for (std::size_t i = 0; i < 4; ++i) {
        const numkit::ASTNode &rhs = *prog->children[i]->children[1];
        EXPECT_TRUE(inferExpr(rhs, env, reg, &creg).type.isDynamic())
            << "object operator/ math expr #" << i << " must be Dynamic, not numeric";
    }
}
