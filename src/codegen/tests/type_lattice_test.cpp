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
    EXPECT_FALSE(InferredType::scalar(ValueType::CHAR).isUnboxableScalar());
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
