// codegen/src/transfer/elementwise.cpp
//
// Transfer functions for the elementwise family: arithmetic and
// comparison/logical binary operators, sign-preserving unary operators,
// real-preserving elementwise math (sin/cos/exp/…), and abs. These share
// two mechanisms — dtype promotion (MATLAB's class rules) and shape
// broadcast.
//
// SOUNDNESS CONTRACT (DESIGN.md §10): every transfer over-approximates —
// the runtime type must be ⊑ the predicted type. A transfer claims a
// precise dtype only on the sub-domain where the operation is provably
// closed over it; elsewhere it returns Dynamic. In particular `power` is
// NOT closed over the reals ((-2)^0.5 is complex), so it is real only for
// a provably-integer exponent and Dynamic otherwise.
//
// Deferred / conservative (documented):
//   * full implicit-expansion broadcast with singleton dims (today:
//     equal-or-scalar shapes are precise, other mismatches -> Unknown);
//   * mtimes matrix shapes beyond the scalar and known-dims cases;
//   * can-go-complex math (sqrt/log/asin/…): NOT registered, so they fall
//     back to Dynamic — sound, because sqrt of a possibly-negative real
//     is complex in MATLAB and must not be mistyped as real.

#include <numkit/codegen/transfer.hpp>

#include <cmath>

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
    // An object operand: the operator may be overloaded by the class, so
    // the result type is the class method's, not a numeric promotion ->
    // Dynamic (sound; never claim 'double' for obj OP obj).
    if (a.dtype == ValueType::OBJECT || b.dtype == ValueType::OBJECT)
        return InferredType::dynamic();
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
    // An object operand: the operator may be overloaded by the class, so
    // the result type is the class method's, not a numeric promotion ->
    // Dynamic (sound; never claim 'double' for obj OP obj).
    if (a.dtype == ValueType::OBJECT || b.dtype == ValueType::OBJECT)
        return InferredType::dynamic();
    if (mixedDistinctIntegers(a.dtype, b.dtype)) return InferredType::dynamic();
    const ValueType dt = promoteArith(a.dtype, b.dtype);
    if (a.shape.isScalar()) return InferredType::concrete(dt, b.shape);
    if (b.shape.isScalar()) return InferredType::concrete(dt, a.shape);
    if (a.shape.kind == ShapeKind::KnownDims && b.shape.kind == ShapeKind::KnownDims)
        return InferredType::concrete(dt, Shape::dims(a.shape.rows, b.shape.cols));
    // matrix * column vector -> column vector (A is m x k, x is k x 1 -> m x 1)
    if (a.shape.kind == ShapeKind::KnownDims && b.shape.kind == ShapeKind::ColVector)
        return InferredType::concrete(dt, Shape::colVector());
    // row vector * matrix -> row vector (r is 1 x k, A is k x n -> 1 x n)
    if (a.shape.kind == ShapeKind::RowVector && b.shape.kind == ShapeKind::KnownDims)
        return InferredType::concrete(dt, Shape::rowVector());
    // row vector * column vector -> scalar (inner / dot product)
    if (a.shape.kind == ShapeKind::RowVector && b.shape.kind == ShapeKind::ColVector)
        return InferredType::scalar(dt);
    return InferredType::concrete(dt, Shape::unknown());
}

// A known integer-valued constant (any sign)? Used to decide when
// real^real stays real.
bool isIntegerConst(const ConstVal &c)
{
    return c.isKnown() && std::isfinite(c.value) && std::floor(c.value) == c.value;
}

// a .^ b — power is NOT closed over the reals: (-2)^0.5 is complex. So
// the result is real only when (Contract 1, soundness):
//   * an operand is already complex -> complex; OR
//   * the exponent is a provably-integer constant -> real (promoted).
// Otherwise the real result could be real OR complex depending on the
// (statically unknown) sign of the base -> Dynamic (neither real nor
// complex over-approximates both in the flat dtype lattice).
InferredType powerTransfer(const std::vector<ArgInfo> &args)
{
    if (args.size() != 2) return InferredType::dynamic();
    const InferredType &a = args[0].type, &b = args[1].type;
    if (!a.isConcrete() || !b.isConcrete()) return InferredType::dynamic();
    // An object operand: the operator may be overloaded by the class, so
    // the result type is the class method's, not a numeric promotion ->
    // Dynamic (sound; never claim 'double' for obj OP obj).
    if (a.dtype == ValueType::OBJECT || b.dtype == ValueType::OBJECT)
        return InferredType::dynamic();
    if (mixedDistinctIntegers(a.dtype, b.dtype)) return InferredType::dynamic();
    if (a.dtype == ValueType::COMPLEX || b.dtype == ValueType::COMPLEX)
        return InferredType::concrete(ValueType::COMPLEX, broadcastShape(a.shape, b.shape));
    if (isIntegerConst(args[1].constant))  // integer exponent -> stays real
        return InferredType::concrete(promoteArith(a.dtype, b.dtype),
                                      broadcastShape(a.shape, b.shape));
    return InferredType::dynamic();  // real base, non-integer exponent -> maybe complex
}

// a ^ b (matrix power). Scalar operands reduce to elementwise power; a
// genuine matrix power needs a square base + integer exponent — deferred.
InferredType mpowerTransfer(const std::vector<ArgInfo> &args)
{
    if (args.size() != 2) return InferredType::dynamic();
    const InferredType &a = args[0].type, &b = args[1].type;
    if (!a.isConcrete() || !b.isConcrete()) return InferredType::dynamic();
    // An object operand: the operator may be overloaded by the class, so
    // the result type is the class method's, not a numeric promotion ->
    // Dynamic (sound; never claim 'double' for obj OP obj).
    if (a.dtype == ValueType::OBJECT || b.dtype == ValueType::OBJECT)
        return InferredType::dynamic();
    if (a.shape.isScalar() && b.shape.isScalar()) return powerTransfer(args);
    return InferredType::dynamic();
}

// ==, <, &, … — always logical, shape broadcast.
InferredType comparisonTransfer(const std::vector<ArgInfo> &args)
{
    if (args.size() != 2) return InferredType::dynamic();
    const InferredType &a = args[0].type, &b = args[1].type;
    if (!a.isConcrete() || !b.isConcrete()) return InferredType::dynamic();
    // An object operand: the operator may be overloaded by the class, so
    // the result type is the class method's, not a numeric promotion ->
    // Dynamic (sound; never claim 'double' for obj OP obj).
    if (a.dtype == ValueType::OBJECT || b.dtype == ValueType::OBJECT)
        return InferredType::dynamic();
    return InferredType::concrete(ValueType::LOGICAL,
                                  broadcastShape(a.shape, b.shape));
}

// -x / +x — preserve dtype+shape, except logical/char promote to double.
InferredType unaryArithTransfer(const std::vector<ArgInfo> &args)
{
    if (args.size() != 1 || !args[0].type.isConcrete()) return InferredType::dynamic();
    if (args[0].type.dtype == ValueType::OBJECT) return InferredType::dynamic();  // may be overloaded
    ValueType dt = args[0].type.dtype;
    if (dt == ValueType::LOGICAL || dt == ValueType::CHAR) dt = ValueType::DOUBLE;
    return InferredType::concrete(dt, args[0].type.shape);
}

// ~x — logical, same shape.
InferredType notTransfer(const std::vector<ArgInfo> &args)
{
    if (args.size() != 1 || !args[0].type.isConcrete()) return InferredType::dynamic();
    if (args[0].type.dtype == ValueType::OBJECT) return InferredType::dynamic();  // may be overloaded
    return InferredType::concrete(ValueType::LOGICAL, args[0].type.shape);
}

// sin/cos/exp/floor/… — real input stays real, complex stays complex,
// same shape. Integer/logical/char input errors in MATLAB (trig needs
// float) -> Dynamic.
InferredType realMathUnaryTransfer(const std::vector<ArgInfo> &args)
{
    if (args.size() != 1 || !args[0].type.isConcrete()) return InferredType::dynamic();
    if (args[0].type.dtype == ValueType::OBJECT) return InferredType::dynamic();  // may be overloaded
    const ValueType dt = args[0].type.dtype;
    if (dt == ValueType::DOUBLE || dt == ValueType::SINGLE || dt == ValueType::COMPLEX)
        return InferredType::concrete(dt, args[0].type.shape);
    return InferredType::dynamic();
}

// atan2/hypot — total on ℝ², no std complex overload. SCALAR real args only
// (the codegen lowers these in scalar context); an array / complex / non-real
// arg -> Dynamic (sound over-approximation). single promotes to double when
// mixed, matching MATLAB.
InferredType realBinaryMathTransfer(const std::vector<ArgInfo> &args)
{
    if (args.size() != 2) return InferredType::dynamic();
    for (const auto &a : args) {
        if (!a.type.isConcrete() || !a.type.shape.isScalar()) return InferredType::dynamic();
        const ValueType dt = a.type.dtype;
        if (dt != ValueType::DOUBLE && dt != ValueType::SINGLE) return InferredType::dynamic();
    }
    const ValueType dt =
        (args[0].type.dtype == ValueType::DOUBLE || args[1].type.dtype == ValueType::DOUBLE)
            ? ValueType::DOUBLE
            : ValueType::SINGLE;
    return InferredType::scalar(dt);
}

// asinh/erf/erfc/expm1 — total on the reals, but with NO std complex overload,
// so a COMPLEX argument is refused (-> Dynamic) rather than emitting a
// non-compiling std::erf(complex). Real (double/single) -> same shape; else
// Dynamic. (The emitter lowers these to std::<name>; see unaryMathStd.)
InferredType realOnlyMathUnaryTransfer(const std::vector<ArgInfo> &args)
{
    if (args.size() != 1 || !args[0].type.isConcrete()) return InferredType::dynamic();
    const ValueType dt = args[0].type.dtype;
    if (dt == ValueType::DOUBLE || dt == ValueType::SINGLE)
        return InferredType::concrete(dt, args[0].type.shape);
    return InferredType::dynamic();
}

// abs — |complex| is real; logical/char promote to double; numeric stays.
InferredType absTransfer(const std::vector<ArgInfo> &args)
{
    if (args.size() != 1 || !args[0].type.isConcrete()) return InferredType::dynamic();
    if (args[0].type.dtype == ValueType::OBJECT) return InferredType::dynamic();  // may be overloaded
    ValueType dt = args[0].type.dtype;
    if (dt == ValueType::COMPLEX || dt == ValueType::LOGICAL || dt == ValueType::CHAR)
        dt = ValueType::DOUBLE;
    return InferredType::concrete(dt, args[0].type.shape);
}

// conj — preserves the value class: complex->complex, real->real (identity).
InferredType conjTransfer(const std::vector<ArgInfo> &args)
{
    if (args.size() != 1 || !args[0].type.isConcrete()) return InferredType::dynamic();
    const ValueType dt = args[0].type.dtype;
    if (dt == ValueType::COMPLEX || dt == ValueType::DOUBLE || dt == ValueType::SINGLE)
        return InferredType::concrete(dt, args[0].type.shape);
    return InferredType::dynamic();
}

// real / imag / angle — the real part / imag part / phase: complex -> real
// (DOUBLE), a real input keeps its real class (real(3.0)=3, imag(3.0)=0). Same
// shape. (std::real / std::imag / std::arg accept double + complex.)
InferredType realPartTransfer(const std::vector<ArgInfo> &args)
{
    if (args.size() != 1 || !args[0].type.isConcrete()) return InferredType::dynamic();
    ValueType dt = args[0].type.dtype;
    if (dt == ValueType::COMPLEX) dt = ValueType::DOUBLE;
    else if (dt != ValueType::DOUBLE && dt != ValueType::SINGLE) return InferredType::dynamic();
    return InferredType::concrete(dt, args[0].type.shape);
}

// fft / ifft — a complex transform; the 1-arg form preserves shape (fft of a
// 1xN vector is a 1xN COMPLEX vector). The emitter has no native form, so the
// call BRIDGES to numkit::fft; this transfer only fixes the result type +
// shape (Contract 1). A real (or zero-imag) ifft result is fine — the complex
// unbox handles a narrowed-to-real result (imag 0). The (x, N) form and the
// matrix-along-dim forms are deferred (-> Dynamic, the sound fallback).
InferredType fftTransfer(const std::vector<ArgInfo> &args)
{
    if (args.size() != 1 || !args[0].type.isConcrete()) return InferredType::dynamic();
    const ValueType dt = args[0].type.dtype;
    if (dt != ValueType::DOUBLE && dt != ValueType::SINGLE && dt != ValueType::COMPLEX)
        return InferredType::dynamic();
    return InferredType::concrete(ValueType::COMPLEX, args[0].type.shape);
}

} // namespace

void registerElementwiseTransfers(TransferRegistry &reg)
{
    // arithmetic (elementwise, closed over reals) — broadcast shape
    for (const char *n : {"plus", "minus", "times", "rdivide", "ldivide"})
        reg.add(n, arithmeticBinaryTransfer);
    // power is NOT closed over reals -> its own (soundness) rule
    reg.add("power", powerTransfer);
    reg.add("mpower", mpowerTransfer);
    // matrix forms — scalar/known-dims shape
    reg.add("mtimes", mtimesTransfer);
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
    // complex accessors
    reg.add("conj", conjTransfer);                       // complex->complex, real->real
    for (const char *n : {"real", "imag", "angle"})      // complex->real (DOUBLE)
        reg.add(n, realPartTransfer);
    for (const char *n : {"fft", "ifft"})                // complex transform (1-arg, bridged)
        reg.add(n, fftTransfer);
    // real-only elementwise math (no std complex overload)
    for (const char *n : {"asinh", "erf", "erfc", "expm1"})
        reg.add(n, realOnlyMathUnaryTransfer);
    // real-only binary math (scalar; no std complex overload)
    for (const char *n : {"atan2", "hypot"})
        reg.add(n, realBinaryMathTransfer);
}

} // namespace numkit::codegen
