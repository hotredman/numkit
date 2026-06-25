// codegen/tests/type_lattice_test.cpp
//
// Unit tests for the codegen type lattice (the inference pass'
// foundation). These pin the lattice algebra — join identities, the
// type-instability collapse, shape generalisation, and the
// "can I emit this as an unboxed C++ scalar?" predicate.

#include <numkit/codegen/type_lattice.hpp>

#include <gtest/gtest.h>

using numkit::ValueType;
using namespace numkit::codegen;

// A concrete scalar of a primitive numeric type is unboxable; aggregates,
// non-scalars, and Dynamic are not.
TEST(TypeLattice, UnboxableScalarPredicate)
{
    EXPECT_TRUE(InferredType::scalar(ValueType::DOUBLE).isUnboxableScalar());
    EXPECT_TRUE(InferredType::scalar(ValueType::COMPLEX).isUnboxableScalar());
    EXPECT_TRUE(InferredType::scalar(ValueType::INT32).isUnboxableScalar());
    EXPECT_TRUE(InferredType::scalar(ValueType::LOGICAL).isUnboxableScalar());

    EXPECT_FALSE(InferredType::dynamic().isUnboxableScalar());
    EXPECT_FALSE(InferredType::bottom().isUnboxableScalar());
    EXPECT_FALSE(InferredType::scalar(ValueType::CELL).isUnboxableScalar());
    EXPECT_TRUE(InferredType::scalar(ValueType::CHAR).isUnboxableScalar());  // 1x1 char = uint16
    // A concrete matrix is concrete but NOT an unboxable scalar.
    EXPECT_FALSE(
        InferredType::concrete(ValueType::DOUBLE, Shape::dims(3, 3)).isUnboxableScalar());
}

// Bottom is the identity of join; Dynamic is absorbing.
TEST(TypeLattice, JoinIdentityAndAbsorbing)
{
    const auto d = InferredType::scalar(ValueType::DOUBLE);

    EXPECT_EQ(join(InferredType::bottom(), d), d);
    EXPECT_EQ(join(d, InferredType::bottom()), d);

    EXPECT_TRUE(join(d, InferredType::dynamic()).isDynamic());
    EXPECT_TRUE(join(InferredType::dynamic(), d).isDynamic());

    // Bottom join Bottom is Bottom.
    EXPECT_TRUE(join(InferredType::bottom(), InferredType::bottom()).isBottom());
}

// Same dtype + same shape on both paths keeps a precise Concrete.
TEST(TypeLattice, JoinSameTypeKeepsConcrete)
{
    const auto a = InferredType::scalar(ValueType::DOUBLE);
    const auto b = InferredType::scalar(ValueType::DOUBLE);
    const auto j = join(a, b);

    EXPECT_TRUE(j.isConcrete());
    EXPECT_EQ(j.dtype, ValueType::DOUBLE);
    EXPECT_TRUE(j.shape.isScalar());
    EXPECT_TRUE(j.isUnboxableScalar());
}

// A type-unstable variable (double on one path, char on another) must
// collapse to Dynamic — i.e. it has to be boxed.
TEST(TypeLattice, JoinDifferentDtypeFallsToDynamic)
{
    const auto j = join(InferredType::scalar(ValueType::DOUBLE),
                        InferredType::scalar(ValueType::CHAR));
    EXPECT_TRUE(j.isDynamic());
}

// Same dtype but different shape: dtype is preserved, shape generalises
// to Unknown — so it is no longer an unboxable scalar.
TEST(TypeLattice, JoinDifferentShapeGeneralisesShape)
{
    const auto s = InferredType::scalar(ValueType::DOUBLE);
    const auto m = InferredType::concrete(ValueType::DOUBLE, Shape::dims(3, 3));
    const auto j = join(s, m);

    ASSERT_TRUE(j.isConcrete());
    EXPECT_EQ(j.dtype, ValueType::DOUBLE);
    EXPECT_EQ(j.shape.kind, ShapeKind::Unknown);
    EXPECT_FALSE(j.isUnboxableScalar());
}

// Two distinct concrete dim sets generalise to Unknown shape too.
TEST(TypeLattice, JoinDistinctDimsToUnknownShape)
{
    const auto a = InferredType::concrete(ValueType::DOUBLE, Shape::dims(3, 3));
    const auto b = InferredType::concrete(ValueType::DOUBLE, Shape::dims(2, 4));
    const auto j = join(a, b);
    ASSERT_TRUE(j.isConcrete());
    EXPECT_EQ(j.shape.kind, ShapeKind::Unknown);
}

// 1x1 KnownDims canonicalises to Scalar so the two never alias.
TEST(TypeLattice, OneByOneDimsCanonicalisesToScalar)
{
    EXPECT_EQ(Shape::dims(1, 1).kind, ShapeKind::Scalar);
    EXPECT_EQ(Shape::dims(1, 1), Shape::scalar());
    // join(scalar, 1x1-dims) stays a scalar (they are equal).
    const auto j = join(InferredType::scalar(ValueType::DOUBLE),
                        InferredType::concrete(ValueType::DOUBLE, Shape::dims(1, 1)));
    EXPECT_TRUE(j.shape.isScalar());
}

// Join is commutative on the cases we rely on.
TEST(TypeLattice, JoinCommutative)
{
    const auto a = InferredType::scalar(ValueType::DOUBLE);
    const auto b = InferredType::concrete(ValueType::DOUBLE, Shape::dims(3, 3));
    EXPECT_EQ(join(a, b), join(b, a));

    const auto c = InferredType::scalar(ValueType::CHAR);
    EXPECT_EQ(join(a, c), join(c, a));
}

// ── N-D ranked shapes (brick N1) ──
TEST(TypeLattice, NDimsRankAndCanonicalisation)
{
    const Shape s = Shape::ndShape({2, 3, 4});
    EXPECT_EQ(s.kind, ShapeKind::NDims);
    EXPECT_TRUE(s.isNDims());
    EXPECT_EQ(s.ndRank(), 3u);
    // A fully-known rank-2 collapses to the existing KnownDims representation.
    EXPECT_EQ(Shape::ndShape({3, 5}).kind, ShapeKind::KnownDims);
    EXPECT_EQ(Shape::ndShape({3, 5}), Shape::dims(3, 5));
    EXPECT_EQ(Shape::ndShape({1, 1}).kind, ShapeKind::Scalar);
    // A rank-2 with a runtime (unknown) dim stays NDims.
    EXPECT_EQ(Shape::ndShape({0, 5}).kind, ShapeKind::NDims);
}

TEST(TypeLattice, NDimsEqualityAndJoin)
{
    using S = Shape;
    EXPECT_EQ(S::ndShape({2, 3, 4}), S::ndShape({2, 3, 4}));
    EXPECT_NE(S::ndShape({2, 3, 4}), S::ndShape({2, 3, 5}));
    EXPECT_EQ(joinShape(S::ndShape({2, 3, 4}), S::ndShape({2, 3, 4})), S::ndShape({2, 3, 4}));
    // one dim differs -> that dim becomes unknown (0), same rank
    const Shape j = joinShape(S::ndShape({2, 3, 4}), S::ndShape({2, 9, 4}));
    ASSERT_TRUE(j.isNDims());
    EXPECT_EQ(j.nd, (std::vector<std::size_t>{2, 0, 4}));
    // different rank (rank-3 NDims vs rank-2 KnownDims) -> Unknown
    EXPECT_EQ(joinShape(S::ndShape({2, 3, 4}), S::ndShape({2, 3})).kind, ShapeKind::Unknown);
}

// ── ConstVal (the SCCP facet that drives shape-from-value) ────────────

// A known non-negative integer constant reads out as an array dimension;
// Unknown, non-integral, and negative values do not.
TEST(ConstLattice, AsDim)
{
    std::size_t d = 999;
    EXPECT_TRUE(ConstVal::known(64.0).asDim(d));
    EXPECT_EQ(d, 64u);

    EXPECT_TRUE(ConstVal::known(0.0).asDim(d));  // 0 is a valid dim
    EXPECT_EQ(d, 0u);

    EXPECT_FALSE(ConstVal::unknown().asDim(d));     // not a constant
    EXPECT_FALSE(ConstVal::known(3.5).asDim(d));    // non-integral
    EXPECT_FALSE(ConstVal::known(-4.0).asDim(d));   // negative
}

// Constant join: equal Known values survive; disagreement → Unknown.
TEST(ConstLattice, Join)
{
    EXPECT_EQ(join(ConstVal::known(7.0), ConstVal::known(7.0)), ConstVal::known(7.0));

    EXPECT_FALSE(join(ConstVal::known(7.0), ConstVal::known(8.0)).isKnown());
    EXPECT_FALSE(join(ConstVal::known(7.0), ConstVal::unknown()).isKnown());
    EXPECT_FALSE(join(ConstVal::unknown(), ConstVal::unknown()).isKnown());
}

// linspace(0, 1, 64): the 3rd argument carries a Known constant, which a
// size-constructor transfer function turns into a concrete column count.
// This is the end-to-end reason ConstVal exists.
TEST(ConstLattice, DrivesShapeFromValue)
{
    const ConstVal n = ConstVal::known(64.0);    // the literal `64`
    std::size_t cols = 0;
    ASSERT_TRUE(n.asDim(cols));
    // What linspace's transfer function would build from it:
    const auto y = InferredType::concrete(ValueType::DOUBLE, Shape::dims(1, cols));
    EXPECT_EQ(y.dtype, ValueType::DOUBLE);
    EXPECT_EQ(y.shape.kind, ShapeKind::KnownDims);
    EXPECT_EQ(y.shape.cols, 64u);

    // A runtime (Unknown) n cannot fix the column count.
    std::size_t dummy = 0;
    EXPECT_FALSE(ConstVal::unknown().asDim(dummy));
}

// ── Struct types (G2.1) ───────────────────────────────────────────────
// A STRUCT InferredType carries a shared field layout (name -> type), supports field
// lookup + a readable str, and is never an unboxable scalar.
TEST(TypeLattice, StructLayoutBasics)
{
    auto lay = std::make_shared<StructLayout>();
    lay->fields = {{"a", InferredType::scalar(ValueType::DOUBLE)},
                   {"b", InferredType::concrete(ValueType::DOUBLE, Shape::dims(1, 3))}};
    const InferredType s = InferredType::structOf(lay);

    EXPECT_TRUE(s.isStruct());
    EXPECT_TRUE(s.isConcrete());
    EXPECT_FALSE(s.isObject());
    EXPECT_FALSE(s.isUnboxableScalar());  // aggregates are not unboxable scalars
    ASSERT_NE(s.structLayout, nullptr);
    ASSERT_NE(s.structLayout->field("a"), nullptr);
    EXPECT_EQ(*s.structLayout->field("a"), InferredType::scalar(ValueType::DOUBLE));
    EXPECT_EQ(s.structLayout->field("missing"), nullptr);
    EXPECT_NE(s.str().find("struct{a,b}"), std::string::npos);
}

// Two structs are equal iff identical layouts (value, not pointer); join keeps an
// identical layout and collapses differing layouts / struct-vs-nonstruct to Dynamic.
TEST(TypeLattice, StructEqualityAndJoin)
{
    auto mk = [](std::vector<std::pair<std::string, InferredType>> f) {
        auto l    = std::make_shared<StructLayout>();
        l->fields = std::move(f);
        return InferredType::structOf(l);
    };
    const InferredType s1 = mk({{"a", InferredType::scalar(ValueType::DOUBLE)}});
    const InferredType s2 = mk({{"a", InferredType::scalar(ValueType::DOUBLE)}});  // same layout, new ptr
    const InferredType s3 = mk({{"a", InferredType::scalar(ValueType::LOGICAL)}});  // field type differs
    const InferredType s4 = mk({{"a", InferredType::scalar(ValueType::DOUBLE)},
                                {"b", InferredType::scalar(ValueType::DOUBLE)}});  // extra field

    EXPECT_EQ(s1, s2);  // value equality, not pointer identity
    EXPECT_NE(s1, s3);
    EXPECT_NE(s1, s4);
    EXPECT_EQ(join(s1, s2), s1);            // identical layout -> itself
    EXPECT_TRUE(join(s1, s3).isDynamic());  // differing field type -> boxed
    EXPECT_TRUE(join(s1, s4).isDynamic());  // differing field set  -> boxed
    EXPECT_TRUE(join(s1, InferredType::scalar(ValueType::DOUBLE)).isDynamic());  // struct vs double
}

// A struct field may itself be a struct (nested s.a.b); equality recurses through it.
TEST(TypeLattice, NestedStruct)
{
    auto inner    = std::make_shared<StructLayout>();
    inner->fields = {{"b", InferredType::scalar(ValueType::DOUBLE)}};
    auto outer    = std::make_shared<StructLayout>();
    outer->fields = {{"a", InferredType::structOf(inner)}};
    const InferredType s = InferredType::structOf(outer);

    ASSERT_NE(s.structLayout->field("a"), nullptr);
    EXPECT_TRUE(s.structLayout->field("a")->isStruct());
    ASSERT_NE(s.structLayout->field("a")->structLayout->field("b"), nullptr);
    EXPECT_EQ(*s.structLayout->field("a")->structLayout->field("b"),
              InferredType::scalar(ValueType::DOUBLE));
}
