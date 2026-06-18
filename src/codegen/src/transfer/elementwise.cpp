// codegen/src/transfer/elementwise.cpp
//
// Transfer functions for the elementwise family: arithmetic and
// comparison/logical binary operators, sign-preserving unary operators,
// real-preserving elementwise math (sin/cos/exp/…), and abs. These share
// two mechanisms — dtype promotion (MATLAB's class rules) and shape
// broadcast.
//
// Deferred / conservative (documented):
//   * full implicit-expansion broadcast with singleton dims (today:
//     equal-or-scalar shapes are precise, other mismatches -> Unknown);
//   * mtimes/mpower matrix shapes beyond the scalar and known-dims cases;
//   * can-go-complex math (sqrt/log/asin/…): NOT registered, so they fall
//     back to Dynamic — sound, because sqrt of a possibly-negative real
//     is complex in MATLAB and must not be mistyped as real.

#include <numkit/codegen/transfer.hpp>

namespace numkit::codegen {

namespace {

// MATLAB arithmetic class promotion (the common cases):
//   any complex            -> complex
//   any integer            -> that integer (integer dominates double)
//   any single             -> single
//   else (double/logical/char under arithmetic) -> double
ValueType promoteArith(ValueType a, ValueType b)
{
    if (a == ValueType::COMPLEX || b == ValueType::COMPLEX) return ValueType::COMPLEX;
    if (isIntegerType(a)) return a;
    if (isIntegerType(b)) return b;
    if (a == ValueType::SINGLE || b == ValueType::SINGLE) return ValueType::SINGLE;
    return ValueType::DOUBLE;
}

// Mixed distinct integer types (int8 + int16) are an error in MATLAB —
// the caller can't soundly type the result.
bool mixedDistinctIntegers(ValueType a, ValueType b)
{
    return isIntegerType(a) && isIntegerType(b) && a != b;
}

// Shape broadcast: equal shapes pass through, a scalar takes the other
// operand's shape; any unproven mismatch collapses to Unknown (full
// singleton-dim expansion is a deferred refinement).
Shape broadcastShape(const Shape &a, const Shape &b)
{
    if (a == b) return a;
    if (a.isScalar()) return b;
    if (b.isScalar()) return a;
    return Shape::unknown();
}

// a + b / a .* b / … — promote dtype, broadcast shape.
InferredType arithmeticBinaryTransfer(const std::vector<ArgInfo> &args)
{
    if (args.size() != 2) return InferredType::dynamic();
    const InferredType &a = args[0].type, &b = args[1].type;
    if (!a.isConcrete() || !b.isConcrete()) return InferredType::dynamic();
    if (mixedDistinctIntegers(a.dtype, b.dtype)) return InferredType::dynamic();
    return InferredType::concrete(promoteArith(a.dtype, b.dtype),
                                  broadcastShape(a.shape, b.shape));
}

// a * b / a ^ b — matrix forms. Scalar operand(s) behave elementwise
// (scalar * X -> X's shape); matrix * matrix -> rows(a) x cols(b) when
// both dims are known; otherwise Unknown shape (dtype still promoted).
InferredType mtimesTransfer(const std::vector<ArgInfo> &args)
{
    if (args.size() != 2) return InferredType::dynamic();
    const InferredType &a = args[0].type, &b = args[1].type;
    if (!a.isConcrete() || !b.isConcrete()) return InferredType::dynamic();
    if (mixedDistinctIntegers(a.dtype, b.dtype)) return InferredType::dynamic();
    const ValueType dt = promoteArith(a.dtype, b.dtype);
    if (a.shape.isScalar()) return InferredType::concrete(dt, b.shape);
    if (b.shape.isScalar()) return InferredType::concrete(dt, a.shape);
    if (a.shape.kind == ShapeKind::KnownDims && b.shape.kind == ShapeKind::KnownDims)
        return InferredType::concrete(dt, Shape::dims(a.shape.rows, b.shape.cols));
    return InferredType::concrete(dt, Shape::unknown());
}

// ==, <, &, … — always logical, shape broadcast.
InferredType comparisonTransfer(const std::vector<ArgInfo> &args)
{
    if (args.size() != 2) return InferredType::dynamic();
    const InferredType &a = args[0].type, &b = args[1].type;
    if (!a.isConcrete() || !b.isConcrete()) return InferredType::dynamic();
    return InferredType::concrete(ValueType::LOGICAL,
                                  broadcastShape(a.shape, b.shape));
}

// -x / +x — preserve dtype+shape, except logical/char promote to double.
InferredType unaryArithTransfer(const std::vector<ArgInfo> &args)
{
    if (args.size() != 1 || !args[0].type.isConcrete()) return InferredType::dynamic();
    ValueType dt = args[0].type.dtype;
    if (dt == ValueType::LOGICAL || dt == ValueType::CHAR) dt = ValueType::DOUBLE;
    return InferredType::concrete(dt, args[0].type.shape);
}

// ~x — logical, same shape.
InferredType notTransfer(const std::vector<ArgInfo> &args)
{
    if (args.size() != 1 || !args[0].type.isConcrete()) return InferredType::dynamic();
    return InferredType::concrete(ValueType::LOGICAL, args[0].type.shape);
}

// sin/cos/exp/floor/… — real input stays real, complex stays complex,
// same shape. Integer/logical/char input errors in MATLAB (trig needs
// float) -> Dynamic.
InferredType realMathUnaryTransfer(const std::vector<ArgInfo> &args)
{
    if (args.size() != 1 || !args[0].type.isConcrete()) return InferredType::dynamic();
    const ValueType dt = args[0].type.dtype;
    if (dt == ValueType::DOUBLE || dt == ValueType::SINGLE || dt == ValueType::COMPLEX)
        return InferredType::concrete(dt, args[0].type.shape);
    return InferredType::dynamic();
}

// abs — |complex| is real; logical/char promote to double; numeric stays.
InferredType absTransfer(const std::vector<ArgInfo> &args)
{
    if (args.size() != 1 || !args[0].type.isConcrete()) return InferredType::dynamic();
    ValueType dt = args[0].type.dtype;
    if (dt == ValueType::COMPLEX || dt == ValueType::LOGICAL || dt == ValueType::CHAR)
        dt = ValueType::DOUBLE;
    return InferredType::concrete(dt, args[0].type.shape);
}

} // namespace

void registerElementwiseTransfers(TransferRegistry &reg)
{
    // arithmetic (elementwise) — broadcast shape
    for (const char *n : {"plus", "minus", "times", "rdivide", "ldivide", "power"})
        reg.add(n, arithmeticBinaryTransfer);
    // matrix forms — scalar/known-dims shape
    reg.add("mtimes", mtimesTransfer);
    reg.add("mpower", mtimesTransfer);
    reg.add("mrdivide", arithmeticBinaryTransfer);  // scalar case is exact
    reg.add("mldivide", arithmeticBinaryTransfer);

    // comparison + logical — always logical
    for (const char *n : {"eq", "ne", "lt", "gt", "le", "ge", "and", "or"})
        reg.add(n, comparisonTransfer);

    // unary
    reg.add("uminus", unaryArithTransfer);
    reg.add("uplus", unaryArithTransfer);
    reg.add("not", notTransfer);

    // real-preserving elementwise math
    for (const char *n : {"sin", "cos", "tan", "exp", "sinh", "cosh", "tanh",
                          "atan", "floor", "ceil", "round", "fix", "sign"})
        reg.add(n, realMathUnaryTransfer);
    reg.add("abs", absTransfer);
}

} // namespace numkit::codegen
