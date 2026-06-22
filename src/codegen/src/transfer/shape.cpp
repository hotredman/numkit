// codegen/src/transfer/shape.cpp
//
// Transfer functions for the shape-query family: numel / length. Both
// return a real non-negative integer count -> a scalar double, regardless
// of the operand (sound and exact: MATLAB numel/length always produce a
// 1x1 double). `size` is deferred: its no-dim form returns a 1xNdims row
// (whose length depends on the operand's rank) and the (x,dim) form a
// scalar -- registering it precisely needs rank tracking, so for now it
// is intentionally left unregistered (-> Dynamic, the sound fallback).

#include <numkit/codegen/transfer.hpp>

namespace numkit::codegen {

namespace {

// numel / length / ndims: always a scalar double count.
InferredType countTransfer(const std::vector<ArgInfo> & /*args*/)
{
    return InferredType::scalar(ValueType::DOUBLE);
}

// size(A, dim) -> a scalar double (one dimension). size(A) (no dim) -> a
// 1 x rank row vector of the dim sizes. rank is MATLAB's: 2 for a scalar /
// vector / matrix, the array's rank for N-D. Unknown shape -> unknown rank ->
// Dynamic (the sound fallback). The VALUES are filled by the emitter; this
// transfer only fixes the result SHAPE (1 x rank).
InferredType sizeTransfer(const std::vector<ArgInfo> &args)
{
    if (args.size() == 2) return InferredType::scalar(ValueType::DOUBLE);  // size(A, dim)
    if (args.size() != 1) return InferredType::dynamic();
    const Shape &sh = args[0].type.shape;
    std::size_t  rank = 0;
    switch (sh.kind) {
    case ShapeKind::Scalar:
    case ShapeKind::RowVector:
    case ShapeKind::ColVector:
    case ShapeKind::KnownDims: rank = 2; break;  // scalars / vectors / matrices are 2-D
    case ShapeKind::NDims:     rank = sh.nd.size(); break;
    case ShapeKind::Unknown:   return InferredType::dynamic();  // unknown rank -> sound fallback
    }
    return InferredType::concrete(ValueType::DOUBLE, Shape::dims(1, rank));  // 1 x rank row
}

// [r, c] = size(A): the two-output size idiom. Both outputs are real scalar
// doubles (r = rows, c = the trailing dims folded). size is variadic, but
// applyMulti is nargout-blind, so v1 models exactly the supported two-output
// arity; the emitter computes the VALUES (folding the trailing dims into c)
// and refuses other nargout. Modelling only the supported shape keeps the
// fallback sound (an unsupported arity -> Dynamic / explicit refusal).
std::vector<InferredType> sizeMultiTransfer(const std::vector<ArgInfo> & /*args*/)
{
    return {InferredType::scalar(ValueType::DOUBLE), InferredType::scalar(ValueType::DOUBLE)};
}

// transpose (A.') / ctranspose (A'): swap the two dimensions. A row becomes a
// column and vice-versa; a matrix's dims swap; a scalar is unchanged. The
// dtype is preserved (ctranspose conjugates the VALUES of a complex operand
// but the type stays complex; the conjugation is emitted, not inferred). N-D
// transpose is undefined in MATLAB -> Dynamic (the sound fallback). Shared by
// both operators since the shape rule is identical.
InferredType transposeTransfer(const std::vector<ArgInfo> &args)
{
    if (args.size() != 1) return InferredType::dynamic();
    const InferredType &t = args[0].type;
    if (!t.isConcrete()) return InferredType::dynamic();
    switch (t.shape.kind) {
    case ShapeKind::Scalar:    return InferredType::concrete(t.dtype, Shape::scalar());
    case ShapeKind::RowVector: return InferredType::concrete(t.dtype, Shape::colVector());
    case ShapeKind::ColVector: return InferredType::concrete(t.dtype, Shape::rowVector());
    case ShapeKind::KnownDims:
        return InferredType::concrete(t.dtype, Shape::dims(t.shape.cols, t.shape.rows));
    case ShapeKind::NDims:
    case ShapeKind::Unknown: return InferredType::dynamic();
    }
    return InferredType::dynamic();
}

// sum / prod / mean / max / min over a VECTOR (single-arg form) -> a scalar of
// the operand's dtype. MATLAB reduces a matrix along a dim (-> a row vector) and
// max/min take a 2nd arg (elementwise) or a 2nd output (index) — only the
// vector->scalar single-output case is typed here (the common reduction); it is
// bridged to the runtime for the exact result (summation order, NaN handling).
// Matrix / N-D input, or a 2-arg call, -> Dynamic (the sound fallback).
InferredType vectorReductionTransfer(const std::vector<ArgInfo> &args)
{
    if (args.size() != 1 || !args[0].type.isConcrete()) return InferredType::dynamic();
    switch (args[0].type.shape.kind) {
    case ShapeKind::Scalar:
    case ShapeKind::RowVector:
    case ShapeKind::ColVector: return InferredType::scalar(args[0].type.dtype);
    default:                   return InferredType::dynamic();
    }
}

} // namespace

void registerShapeTransfers(TransferRegistry &reg)
{
    reg.add("numel", countTransfer);
    reg.add("length", countTransfer);
    reg.add("ndims", countTransfer);
    reg.add("size", sizeTransfer);
    reg.addMulti("size", sizeMultiTransfer);
    reg.add("transpose", transposeTransfer);   // A.'
    reg.add("ctranspose", transposeTransfer);  // A'
    // Vector reductions -> scalar (bridged; the emitter boxes the array arg).
    for (const char *n : {"sum", "prod", "mean", "max", "min"})
        reg.add(n, vectorReductionTransfer);
}

} // namespace numkit::codegen
