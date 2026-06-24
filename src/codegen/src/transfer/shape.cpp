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

// [m, i] = max(x) / min(x): two outputs -- the extremum VALUE and its 1-based INDEX
// (both real scalar doubles). Typed concrete ONLY for a DOUBLE VECTOR, which the
// emitter lowers natively (a scan tracking the extremum + its position) in EVERY
// tier -- so the transfer and the always-native emit agree. A matrix/N-D or non-
// double operand -> Dynamic, which routes to the bridged multi-output path.
std::vector<InferredType> maxMinMultiTransfer(const std::vector<ArgInfo> &args)
{
    if (args.size() != 1 || !args[0].type.isConcrete()
        || args[0].type.dtype != ValueType::DOUBLE)
        return {InferredType::dynamic(), InferredType::dynamic()};
    switch (args[0].type.shape.kind) {
    case ShapeKind::Scalar:
    case ShapeKind::RowVector:
    case ShapeKind::ColVector:
    case ShapeKind::Unknown:  // a 1-D buffer of unknown length is still a vector
        return {InferredType::scalar(ValueType::DOUBLE), InferredType::scalar(ValueType::DOUBLE)};
    default:  // matrix / N-D reduces to a row, not two scalars -> deferred
        return {InferredType::dynamic(), InferredType::dynamic()};
    }
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
        // A rank-2 matrix (incl. a runtime-dim 2-D) -> swap the two dims. ndShape keeps
        // it NDims when a dim is runtime, or canonicalises to KnownDims if both are now
        // known. A true N-D (rank>=3) transpose is undefined in MATLAB -> Dynamic.
        if (t.shape.nd.size() == 2)
            return InferredType::concrete(t.dtype,
                                          Shape::ndShape({t.shape.nd[1], t.shape.nd[0]}));
        return InferredType::dynamic();
    case ShapeKind::Unknown: return InferredType::dynamic();
    }
    return InferredType::dynamic();
}

// rot90(A): rotate a 2-D matrix 90 degrees -> the dims SWAP (m x n -> n x m), like
// transpose. Only the 1-arg matrix forms are typed (KnownDims rank-2 / NDims rank-2); a
// vector / scalar / the rot90(A,k) 2-arg form -> Dynamic (bridged) -- the emit handles the
// 1-arg 2-D case only.
InferredType rot90Transfer(const std::vector<ArgInfo> &args)
{
    if (args.size() != 1 || !args[0].type.isConcrete()) return InferredType::dynamic();
    const Shape &s = args[0].type.shape;
    if (s.kind == ShapeKind::KnownDims)
        return InferredType::concrete(args[0].type.dtype, Shape::dims(s.cols, s.rows));
    if (s.kind == ShapeKind::NDims && s.nd.size() == 2)
        return InferredType::concrete(args[0].type.dtype, Shape::ndShape({s.nd[1], s.nd[0]}));
    return InferredType::dynamic();
}

// kron(A,B): Kronecker product. A m x n, B p x q -> (m*p) x (n*q). Both operands must be
// 2-D matrices (KnownDims rank-2 or runtime-dim NDims rank-2), DOUBLE; the result is a
// runtime-dim 2-D (ndShape({0,0})). Vector / scalar / non-DOUBLE operands -> Dynamic
// (bridged); kron ships in the linalg toolbox, so the bridge handles those.
InferredType kronTransfer(const std::vector<ArgInfo> &args)
{
    if (args.size() != 2 || !args[0].type.isConcrete() || !args[1].type.isConcrete())
        return InferredType::dynamic();
    auto isMat = [](const Shape &s) {
        return (s.kind == ShapeKind::KnownDims && s.rows > 1 && s.cols > 1)
               || (s.kind == ShapeKind::NDims && s.nd.size() == 2);
    };
    if (isMat(args[0].type.shape) && isMat(args[1].type.shape)
        && args[0].type.dtype == ValueType::DOUBLE && args[1].type.dtype == ValueType::DOUBLE)
        return InferredType::concrete(ValueType::DOUBLE, Shape::ndShape({0, 0}));
    return InferredType::dynamic();
}

// cat(dim, A, B): concatenate along dim -- dim==1 vertical (like [A;B]), dim==2 horizontal
// (like [A B]). v1: a LITERAL dim 1|2 and exactly two 2-D DOUBLE matrix operands -> a
// runtime-dim 2-D result (ndShape({0,0})). A runtime dim / >2 operands / vector / non-DOUBLE
// -> Dynamic (bridged).
InferredType catTransfer(const std::vector<ArgInfo> &args)
{
    if (args.size() != 3) return InferredType::dynamic();  // dim + exactly two arrays (v1)
    std::size_t dim = 0;
    if (!args[0].constant.asDim(dim) || (dim != 1 && dim != 2)) return InferredType::dynamic();
    auto isMat = [](const Shape &s) {
        return (s.kind == ShapeKind::KnownDims && s.rows > 1 && s.cols > 1)
               || (s.kind == ShapeKind::NDims && s.nd.size() == 2);
    };
    if (args[1].type.isConcrete() && args[2].type.isConcrete()
        && isMat(args[1].type.shape) && isMat(args[2].type.shape)
        && args[1].type.dtype == ValueType::DOUBLE && args[2].type.dtype == ValueType::DOUBLE)
        return InferredType::concrete(ValueType::DOUBLE, Shape::ndShape({0, 0}));
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

// max / min: BOTH the 1-arg vector reduction (-> scalar, as vectorReductionTransfer) AND the
// 2-arg ELEMENTWISE form max(a,b) / min(a,b). MATLAB's 2-arg max/min ignore NaN (the result
// is the non-NaN operand) -- bit-identical to std::fmax/std::fmin, so the emitter lowers them
// via binaryMathStd (scalar) and the elementwise fill (arrays). REAL DOUBLE operands only
// (complex max is by magnitude; integer/single/object differ) -> else Dynamic (bridged). The
// 2-arg result is typed concrete ONLY for equal shapes or a scalar broadcast; any other shape
// mismatch (row vs col, vector vs matrix -> MATLAB implicit expansion) -> Dynamic, so the
// runtime does the broadcast (never a wrong-shape miscompile). 3-arg max(x,[],dim) -> Dynamic.
InferredType maxMinTransfer(const std::vector<ArgInfo> &args)
{
    if (args.empty() || !args[0].type.isConcrete()) return InferredType::dynamic();
    if (args.size() == 1) {  // vector reduction -> scalar
        switch (args[0].type.shape.kind) {
        case ShapeKind::Scalar:
        case ShapeKind::RowVector:
        case ShapeKind::ColVector: return InferredType::scalar(args[0].type.dtype);
        default:                   return InferredType::dynamic();
        }
    }
    if (args.size() == 2 && args[1].type.isConcrete()) {  // elementwise max(a,b) / min(a,b)
        if (args[0].type.dtype != ValueType::DOUBLE || args[1].type.dtype != ValueType::DOUBLE)
            return InferredType::dynamic();
        const Shape &sa = args[0].type.shape, &sb = args[1].type.shape;
        if (sa == sb)      return InferredType::concrete(ValueType::DOUBLE, sa);
        if (sa.isScalar()) return InferredType::concrete(ValueType::DOUBLE, sb);
        if (sb.isScalar()) return InferredType::concrete(ValueType::DOUBLE, sa);
        return InferredType::dynamic();  // shape mismatch / implicit expansion -> bridged
    }
    return InferredType::dynamic();  // 3+ args (max(x,[],dim), max(x,[],"all")) -> bridged
}

// any / all over a VECTOR -> a LOGICAL scalar (true/false), regardless of the
// operand dtype. Only the vector->scalar single-output case is typed (the emitter
// lowers it to a native inline short-circuit loop). Matrix / N-D / 2-arg -> Dynamic.
InferredType anyAllTransfer(const std::vector<ArgInfo> &args)
{
    if (args.size() != 1 || !args[0].type.isConcrete()) return InferredType::dynamic();
    switch (args[0].type.shape.kind) {
    case ShapeKind::Scalar:
    case ShapeKind::RowVector:
    case ShapeKind::ColVector: return InferredType::scalar(ValueType::LOGICAL);
    default:                   return InferredType::dynamic();
    }
}

// find(x) -> a 1-D DOUBLE vector of the 1-based positions of the nonzero
// elements. The length is runtime (-> Unknown shape, still a buffer type); the
// emitter lowers it to a native filter loop. Single-arg vector form only; a
// matrix / N-D input or a 2-arg call -> Dynamic.
InferredType findTransfer(const std::vector<ArgInfo> &args)
{
    if (args.size() != 1 || !args[0].type.isConcrete()) return InferredType::dynamic();
    // find returns a 1-D vector of the 1-based LINEAR (column-major) positions of the
    // nonzero/true elements, regardless of the operand's rank -- a vector OR a matrix / N-D
    // array. The emit filters flat over the column-major buffer (the flat index +1 is the
    // MATLAB linear index), so every ranked shape types to a 1-D DOUBLE result.
    switch (args[0].type.shape.kind) {
    case ShapeKind::Scalar:
    case ShapeKind::RowVector:
    case ShapeKind::ColVector:
    case ShapeKind::Unknown:  // a 1-D buffer of unknown length is still a vector
    case ShapeKind::KnownDims:
    case ShapeKind::NDims:
        return InferredType::concrete(ValueType::DOUBLE, Shape::unknown());
    }
    return InferredType::dynamic();
}

// diff(x) -> consecutive differences, a 1-D vector of length n-1 (runtime ->
// Unknown shape) preserving the operand dtype. Single-arg vector form only; a
// scalar (diff -> empty), matrix / N-D input, or a 2-arg call -> Dynamic.
InferredType diffTransfer(const std::vector<ArgInfo> &args)
{
    if (args.size() != 1 || !args[0].type.isConcrete()) return InferredType::dynamic();
    switch (args[0].type.shape.kind) {
    case ShapeKind::RowVector:
    case ShapeKind::ColVector:
    case ShapeKind::Unknown:  // a 1-D buffer of unknown length is still a vector
        return InferredType::concrete(args[0].type.dtype, Shape::unknown());
    default: return InferredType::dynamic();
    }
}

// dot(a,b) -> scalar inner product (sum of a.*b for real). v1: two 1-D real
// DOUBLE vectors -> a DOUBLE scalar; the emitter lowers it to an accumulation
// loop. Anything else (complex needs conj, matrix, wrong arity) -> Dynamic.
InferredType dotTransfer(const std::vector<ArgInfo> &args)
{
    if (args.size() != 2 || !args[0].type.isConcrete() || !args[1].type.isConcrete())
        return InferredType::dynamic();
    if (args[0].type.dtype != ValueType::DOUBLE || args[1].type.dtype != ValueType::DOUBLE)
        return InferredType::dynamic();
    auto isVec = [](ShapeKind k) {
        return k == ShapeKind::RowVector || k == ShapeKind::ColVector
               || k == ShapeKind::Unknown || k == ShapeKind::Scalar;
    };
    if (isVec(args[0].type.shape.kind) && isVec(args[1].type.shape.kind))
        return InferredType::scalar(ValueType::DOUBLE);
    return InferredType::dynamic();
}

// strcmp(a,b) -> a LOGICAL scalar (true iff the two arrays are identical: same
// length AND elementwise equal). v1: two concrete array args -> LOGICAL scalar;
// the emitter lowers it to a length + elementwise compare.
InferredType strcmpTransfer(const std::vector<ArgInfo> &args)
{
    if (args.size() != 2 || !args[0].type.isConcrete() || !args[1].type.isConcrete())
        return InferredType::dynamic();
    return InferredType::scalar(ValueType::LOGICAL);
}

// Single-arg type/shape query predicates (isempty / isscalar / isreal): always a
// LOGICAL scalar for a concrete arg. The emitter computes the boolean VALUE --
// compile-time for dtype queries (isreal: the dtype is static), a numel compare
// for shape queries (isempty: ==0, isscalar: ==1), a compile-time constant for a
// scalar operand. A non-concrete (Dynamic) arg -> Dynamic, the sound fallback.
// Pure expressions -> the emitter can place them in any context (incl. an `if`).
InferredType logicalQueryTransfer(const std::vector<ArgInfo> &args)
{
    if (args.size() != 1 || !args[0].type.isConcrete()) return InferredType::dynamic();
    return InferredType::scalar(ValueType::LOGICAL);
}

// sort(x) / sort(x, 'ascend'|'descend') -> same shape AND dtype as x (the values
// reorder; the shape/dtype don't). The optional 2nd arg is a CHAR direction string;
// only these forms are typed. sort along a numeric dim, or the 2-output [s,i]=sort,
// are deferred -> Dynamic (the sound fallback).
InferredType sortTransfer(const std::vector<ArgInfo> &args)
{
    if (args.empty() || args.size() > 2 || !args[0].type.isConcrete())
        return InferredType::dynamic();
    if (args.size() == 2 && args[1].type.dtype != ValueType::CHAR)
        return InferredType::dynamic();  // a numeric dim arg -> deferred
    return args[0].type;
}

// polyval(p, x) -> the polynomial with coefficients p (highest degree first)
// evaluated at each point of x, a DOUBLE result with the SAME shape as x. v1: x a
// VECTOR (Horner per element); a scalar / matrix x is deferred -> Dynamic. (p is the
// coefficient vector; the result tracks x's shape.)
InferredType polyvalTransfer(const std::vector<ArgInfo> &args)
{
    if (args.size() != 2 || !args[0].type.isConcrete() || !args[1].type.isConcrete())
        return InferredType::dynamic();
    switch (args[1].type.shape.kind) {
    case ShapeKind::RowVector:
    case ShapeKind::ColVector:
    case ShapeKind::Unknown:  // a 1-D buffer of unknown length is still a vector
        return InferredType::concrete(ValueType::DOUBLE, args[1].type.shape);
    default: return InferredType::dynamic();  // scalar / matrix x -> deferred
    }
}

// cross(a, b): the 3-D vector cross product -> a 3-element vector (1-D DOUBLE). v1:
// two concrete 1-D DOUBLE vectors (length 3 assumed -- a non-3 operand is a runtime
// error in MATLAB, not modelled). 2-D operands or a dim arg -> Dynamic.
InferredType crossTransfer(const std::vector<ArgInfo> &args)
{
    if (args.size() != 2 || !args[0].type.isConcrete() || !args[1].type.isConcrete())
        return InferredType::dynamic();
    if (args[0].type.dtype != ValueType::DOUBLE || args[1].type.dtype != ValueType::DOUBLE)
        return InferredType::dynamic();
    auto isVec = [](ShapeKind k) {
        return k == ShapeKind::RowVector || k == ShapeKind::ColVector || k == ShapeKind::Unknown;
    };
    if (isVec(args[0].type.shape.kind) && isVec(args[1].type.shape.kind))
        return InferredType::concrete(ValueType::DOUBLE, Shape::unknown());  // 1-D, length 3
    return InferredType::dynamic();
}

// trace(A): the sum of the diagonal of a 2-D matrix -> a DOUBLE scalar. v1: a 2-D
// matrix operand; a non-2-D operand -> Dynamic.
InferredType traceTransfer(const std::vector<ArgInfo> &args)
{
    if (args.size() != 1 || !args[0].type.isConcrete()) return InferredType::dynamic();
    if (args[0].type.shape.kind == ShapeKind::KnownDims) return InferredType::scalar(ValueType::DOUBLE);
    return InferredType::dynamic();
}

// diag(A): the diagonal of a 2-D matrix as a 1-D vector (length min(rows,cols),
// runtime -> Unknown shape). v1: a 2-D matrix operand. diag(v) (a vector -> a
// diagonal MATRIX) and diag(A,k) (the k-th diagonal) are deferred -> Dynamic.
InferredType diagTransfer(const std::vector<ArgInfo> &args)
{
    if (args.empty() || args.size() > 2 || !args[0].type.isConcrete())
        return InferredType::dynamic();
    // diag(A) (1-arg matrix) -> its main diagonal (1-D). diag(A,k) on a matrix is deferred
    // (k-dependent length) -> Dynamic (bridged).
    if (args[0].type.shape.kind == ShapeKind::KnownDims)
        return args.size() == 1 ? InferredType::concrete(args[0].type.dtype, Shape::unknown())
                                : InferredType::dynamic();
    // diag(v) / diag(v,k) with v a VECTOR -> a diagonal MATRIX: an N x N runtime-dim 2-D
    // (N = numel(v) + |k|). Modelled as a rank-2 NDims with both dims runtime (ndShape
    // ({0,0}) -> a rank-2 ndRuntimeLocal). The emit handles a literal k offset.
    switch (args[0].type.shape.kind) {
    case ShapeKind::RowVector:
    case ShapeKind::ColVector:
    case ShapeKind::Unknown:
        // A runtime (non-literal) k stays Dynamic -> bridged (the emit needs a literal k
        // to size the N x N matrix and decide the diagonal direction at compile time).
        if (args.size() == 2 && !args[1].constant.isKnown()) return InferredType::dynamic();
        return InferredType::concrete(args[0].type.dtype, Shape::ndShape({0, 0}));
    default:
        return InferredType::dynamic();  // scalar / N-D deferred
    }
}

// unique(x) -> the sorted distinct values, a 1-D vector of runtime length (-> Unknown
// shape) preserving the operand dtype. The emitter sorts a copy then dedups
// consecutive-equal elements (NaN are KEPT -- MATLAB treats each NaN as distinct --
// because NaN != NaN). v1: a 1-D vector; a matrix/N-D operand -> Dynamic.
InferredType uniqueTransfer(const std::vector<ArgInfo> &args)
{
    if (args.size() != 1 || !args[0].type.isConcrete()) return InferredType::dynamic();
    switch (args[0].type.shape.kind) {
    case ShapeKind::Scalar:
    case ShapeKind::RowVector:
    case ShapeKind::ColVector:
    case ShapeKind::Unknown:
        return InferredType::concrete(args[0].type.dtype, Shape::unknown());
    default: return InferredType::dynamic();
    }
}

// circshift(x, k) -> same shape AND dtype as x (a circular permutation of the
// elements; the 2nd arg is the integer shift). v1: the (array, scalar-shift) form;
// a shift VECTOR (per-dim) or a 2-D/N-D operand is deferred -> Dynamic.
InferredType circshiftTransfer(const std::vector<ArgInfo> &args)
{
    if ((args.size() != 2 && args.size() != 3) || !args[0].type.isConcrete())
        return InferredType::dynamic();
    if (args[1].type.isConcrete() && args[1].type.shape.kind != ShapeKind::Scalar)
        return InferredType::dynamic();  // a per-dim shift vector -> deferred
    // circshift(A, k, dim): only a known literal dim 1 or 2 is native (the emit needs the
    // axis at compile time). A runtime dim, or any other literal, stays Dynamic (bridged --
    // the runtime handles dim >= 3 as a no-op and dim <= 0 as the MATLAB subscript error).
    if (args.size() == 3
        && !(args[2].constant.isKnown()
             && (args[2].constant.value == 1.0 || args[2].constant.value == 2.0)))
        return InferredType::dynamic();
    return args[0].type;
}

// cumsum / cumprod / flip: shape- AND dtype-preserving (one array in, the same
// array shape out). Single-arg form only. Bridged via the array-result path
// (the runtime owns the algorithm); the value matches the interpreter exactly.
InferredType identityShapeTransfer(const std::vector<ArgInfo> &args)
{
    if (args.size() != 1 || !args[0].type.isConcrete()) return InferredType::dynamic();
    return args[0].type;
}

// tril / triu: lower / upper triangular part -> SAME shape as A, with an optional
// diagonal-offset 2nd arg (tril(A,k) / triu(A,k)). Shape is A's regardless of k, so a
// 1-OR-2-arg identity-shape transfer (identityShapeTransfer is strictly 1-arg).
InferredType trilTriuTransfer(const std::vector<ArgInfo> &args)
{
    if (args.empty() || args.size() > 2 || !args[0].type.isConcrete())
        return InferredType::dynamic();
    return args[0].type;
}

} // namespace

void registerShapeTransfers(TransferRegistry &reg)
{
    reg.add("numel", countTransfer);
    reg.add("length", countTransfer);
    reg.add("ndims", countTransfer);
    reg.add("size", sizeTransfer);
    reg.addMulti("size", sizeMultiTransfer);
    reg.addMulti("max", maxMinMultiTransfer);  // [m,i]=max(x) -> {value, 1-based index}
    reg.addMulti("min", maxMinMultiTransfer);  // [m,i]=min(x) -> {value, 1-based index}
    reg.add("transpose", transposeTransfer);   // A.'
    reg.add("ctranspose", transposeTransfer);  // A'
    reg.add("rot90", rot90Transfer);           // rot90(A) -> 90deg rotation (dims swap)
    reg.add("kron", kronTransfer);             // kron(A,B) -> Kronecker product (mp x nq)
    reg.add("cat", catTransfer);               // cat(dim,A,B) literal dim -> vert/horz concat
    // Vector reductions -> scalar (bridged; the emitter boxes the array arg).
    for (const char *n :
         {"sum", "prod", "mean", "norm", "std", "var", "median", "trapz"})
        reg.add(n, vectorReductionTransfer);
    // max/min: 1-arg reduction (-> scalar) OR 2-arg elementwise max(a,b)/min(a,b) (native
    // via fmax/fmin). The single-output transfer; [m,i]=max(x) uses maxMinMultiTransfer.
    reg.add("max", maxMinTransfer);
    reg.add("min", maxMinTransfer);
    for (const char *n : {"any", "all"})  // vector -> LOGICAL scalar (native inline loop)
        reg.add(n, anyAllTransfer);
    reg.add("find", findTransfer);  // vector -> 1-D DOUBLE positions (native filter loop)
    reg.add("diff", diffTransfer);  // vector -> 1-D differences, length n-1 (native loop)
    reg.add("dot", dotTransfer);    // (vec, vec) -> DOUBLE scalar inner product (native loop)
    reg.add("strcmp", strcmpTransfer);  // (arr, arr) -> LOGICAL scalar string equality (native)
    reg.add("circshift", circshiftTransfer);  // (x, k) -> same shape (native circular shift)
    reg.add("diag", diagTransfer);  // diag(A) -> the diagonal of a matrix as a 1-D vector
    reg.add("trace", traceTransfer);  // trace(A) -> sum of the diagonal (DOUBLE scalar)
    reg.add("cross", crossTransfer);  // cross(a,b) -> the 3-D cross product (1-D, len 3)
    reg.add("unique", uniqueTransfer);  // x -> sorted distinct 1-D (native sort + dedup)
    reg.add("polyval", polyvalTransfer);  // (p, x) -> p evaluated at x, shape of x (Horner)
    // x -> LOGICAL scalar query predicates (native: compile-time / numel / orientation).
    for (const char *n : {"isempty", "isscalar", "isreal", "isrow", "iscolumn", "isvector"})
        reg.add(n, logicalQueryTransfer);
    // dtype-classification predicates (compile-time constants from the static dtype).
    for (const char *n : {"isnumeric", "isfloat", "isinteger", "ischar", "islogical"})
        reg.add(n, logicalQueryTransfer);
    // Shape-preserving array->array builtins (bridged via the array-result path).
    for (const char *n : {"cumsum", "cumprod", "cummax", "cummin", "flip", "fliplr", "flipud",
                          "gradient"})
        reg.add(n, identityShapeTransfer);
    reg.add("tril", trilTriuTransfer);  // tril(A[, k]) -- same shape, optional diag offset
    reg.add("triu", trilTriuTransfer);  // triu(A[, k])
    for (const char *n : {"upper", "lower"})  // char case transform (native, same shape)
        reg.add(n, identityShapeTransfer);
    reg.add("sort", sortTransfer);  // sort(x[, 'ascend'|'descend']) -> same shape (native std::sort)
}

} // namespace numkit::codegen
