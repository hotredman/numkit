// codegen/src/transfer/constructors.cpp
//
// Transfer functions for the size-constructor family: zeros / ones /
// linspace. These share the defining trait that their result *shape*
// comes from argument *values* (a known-constant dimension argument
// gives a KnownDims shape; a runtime one falls back to Unknown shape but
// keeps the dtype). dtype is fixed (double) in the MVP.
//
// Deferred (documented gaps, to be added with validation):
//   * a trailing class-name arg (zeros(3,'int8')) changing the dtype;
//   * linspace single/complex endpoint promotion (single->single,
//     complex endpoint->complex);
//   * a "row vector, length unknown" shape (today a runtime length
//     collapses to Unknown shape — sound, just less precise).

#include <numkit/codegen/transfer.hpp>

namespace numkit::codegen {

namespace {

// zeros / ones: dtype double; shape from the dimension arguments.
//   ()        -> 1x1 scalar
//   (n)       -> n x n           (KnownDims iff n is a known constant)
//   (m, n)    -> m x n           (KnownDims iff both are known constants)
//   (>=3 dims)-> Unknown shape   (N-D beyond the 2-D MVP)
InferredType zerosOnesTransfer(const std::vector<ArgInfo> &args)
{
    Shape sh;
    std::size_t a = 0, b = 0;
    if (args.empty()) {
        sh = Shape::scalar();
    } else if (args.size() == 1) {
        sh = args[0].constant.asDim(a) ? Shape::dims(a, a) : Shape::unknown();
    } else {
        // >= 2 dims: a ranked array of rank = nargs. Each known-constant dim records
        // its exact size; a runtime dim records 0 (unknown). The shape stays NDims
        // (ranked) rather than collapsing to Unknown -- sound (over-approximates an
        // unknown dim), and it lets the emitter materialise runtime dim vars from the
        // call args (a runtime-dim local, incl. a RUNTIME-DIM 2-D matrix). ndShape
        // canonicalises a fully-known rank-2 back to KnownDims (the const-dim path).
        std::vector<std::size_t> dimsv;
        for (const auto &arg : args) {
            std::size_t d = 0;
            dimsv.push_back(arg.constant.asDim(d) ? d : 0);
        }
        sh = Shape::ndShape(std::move(dimsv));
    }
    return InferredType::concrete(ValueType::DOUBLE, sh);
}

// linspace(a, b [, n]) -> 1 x N double row.
//   n a known constant -> 1 x n ;  2-arg form -> 1 x 100 (MATLAB default);
//   runtime n -> Unknown shape (still double; "row, unknown length" is a
//   deferred shape refinement).
InferredType linspaceTransfer(const std::vector<ArgInfo> &args)
{
    Shape       sh;
    std::size_t n = 0;
    if (args.size() >= 3 && args[2].constant.asDim(n))
        sh = Shape::dims(1, n);
    else if (args.size() == 2)
        sh = Shape::dims(1, 100);
    else
        sh = Shape::unknown();
    return InferredType::concrete(ValueType::DOUBLE, sh);
}

// logspace(a, b [, n]) -> 1 x N double row of decade-spaced points (10^linspace).
// Same shape rule as linspace EXCEPT the 2-arg default is n=50 (MATLAB / numkit
// logspace), not 100. n a known constant -> 1 x n; runtime n -> Unknown shape.
InferredType logspaceTransfer(const std::vector<ArgInfo> &args)
{
    Shape       sh;
    std::size_t n = 0;
    if (args.size() >= 3 && args[2].constant.asDim(n))
        sh = Shape::dims(1, n);
    else if (args.size() == 2)
        sh = Shape::dims(1, 50);  // MATLAB logspace default n=50
    else
        sh = Shape::unknown();
    return InferredType::concrete(ValueType::DOUBLE, sh);
}

// eye(n) / eye(m, n) -> the identity matrix; dtype double, shape from the dim args
// (like zeros/ones). () -> 1x1 (the scalar 1); (n) -> n x n; (m,n) -> m x n. A known-
// constant dim gives KnownDims; a RUNTIME dim gives an NDims rank-2 (a runtime-dim 2-D
// the emitter materialises as an ndRuntimeLocal, like zeros(m,n) runtime). >=3 dims ->
// Unknown.
InferredType eyeTransfer(const std::vector<ArgInfo> &args)
{
    std::size_t a = 0;
    if (args.empty()) return InferredType::concrete(ValueType::DOUBLE, Shape::scalar());
    if (args.size() == 1)  // eye(n): n x n -- both dims are the same n
        return args[0].constant.asDim(a)
                   ? InferredType::concrete(ValueType::DOUBLE, Shape::dims(a, a))
                   : InferredType::concrete(ValueType::DOUBLE, Shape::ndShape({0, 0}));
    if (args.size() == 2) {
        // const dim -> its value; runtime dim -> 0. ndShape canonicalises a fully-known
        // rank-2 back to KnownDims (the const path) and keeps a runtime one as NDims.
        std::vector<std::size_t> dimsv;
        for (const auto &arg : args) {
            std::size_t d = 0;
            dimsv.push_back(arg.constant.asDim(d) ? d : 0);
        }
        return InferredType::concrete(ValueType::DOUBLE, Shape::ndShape(std::move(dimsv)));
    }
    return InferredType::concrete(ValueType::DOUBLE, Shape::unknown());
}

// reshape(x, m, n) -> reinterpret x as an m x n matrix (column-major, the SAME flat
// data, so numel must match). v1: m,n known constants -> KnownDims(m, n) of x's
// dtype; a runtime dim -> Dynamic (deferred). The reshape(x,[m n]) vector-size form
// and the []-placeholder form are deferred.
InferredType reshapeTransfer(const std::vector<ArgInfo> &args)
{
    if (args.size() < 3 || !args[0].type.isConcrete()) return InferredType::dynamic();
    // reshape(x, d1, ..., dN) with N >= 3 dims -> a RANK-N array (same column-major flat data).
    // Always typed as a runtime-dim NDims of rank N = nargs-1 (the emit reads the dims from the
    // args and guards numel); a scalar operand has no data to reinterpret -> Dynamic.
    if (args.size() >= 4) {
        if (args[0].type.shape.isScalar()) return InferredType::dynamic();
        return InferredType::concrete(
            args[0].type.dtype, Shape::ndShape(std::vector<std::size_t>(args.size() - 1, 0)));
    }
    std::size_t m = 0, n = 0;
    if (args[1].constant.asDim(m) && args[2].constant.asDim(n))
        return InferredType::concrete(args[0].type.dtype, Shape::dims(m, n));
    // A runtime dim -> a runtime-dim 2-D (NDims rank-2; the emitter copies x's flat
    // buffer, sets the dims, and guards numel). x must be a non-scalar array (its data is
    // reinterpreted). ndShape keeps it NDims while a dim is runtime.
    std::size_t rm = 0, rn = 0;
    if (!args[0].type.shape.isScalar())
        return InferredType::concrete(args[0].type.dtype,
                                      Shape::ndShape({args[1].constant.asDim(rm) ? rm : 0,
                                                      args[2].constant.asDim(rn) ? rn : 0}));
    return InferredType::dynamic();
}

// repmat(A, m, n) / repmat(A, n) -> tile A into an (m*rows) x (n*cols) matrix. v1: A
// a SCALAR -> the result is m x n (or n x n) all = A (tiling a 1x1). Literal dims ->
// KnownDims of A's dtype. A vector/matrix operand (true tiling) or runtime dims are
// deferred -> Dynamic.
InferredType repmatTransfer(const std::vector<ArgInfo> &args)
{
    if (args.empty() || !args[0].type.isConcrete()) return InferredType::dynamic();
    std::size_t m = 0, n = 0;
    // Scalar operand -> an m x n (or n x n) matrix all = s.
    if (args[0].type.shape.kind == ShapeKind::Scalar) {
        if (args.size() == 2 && args[1].constant.asDim(m))  // repmat(s, n) -> n x n
            return InferredType::concrete(args[0].type.dtype, Shape::dims(m, m));
        if (args.size() == 3 && args[1].constant.asDim(m) && args[2].constant.asDim(n))
            return InferredType::concrete(args[0].type.dtype, Shape::dims(m, n));
        return InferredType::dynamic();
    }
    // Row-vector operand: repmat(rowVec, p, q) tiles a 1 x n row into p x (q*n).
    //   p == 1  -> a 1 x (q*n) row (q copies concatenated), a 1-D row (Unknown shape).
    //   p  > 1  -> a true 2-D p x (q*n) matrix. rows = p (known), cols = q*n (runtime,
    //              n is the operand's runtime length) -> NDims rank-2 ndShape({p, 0}),
    //              a runtime-dim 2-D the emitter materialises as an ndRuntimeLocal.
    if (args[0].type.shape.kind == ShapeKind::RowVector && args.size() == 3) {
        std::size_t p = 0, q = 0;
        if (args[1].constant.asDim(p) && p == 1)
            return InferredType::concrete(args[0].type.dtype, Shape::unknown());
        if (args[1].constant.asDim(p) && args[2].constant.asDim(q) && p > 1)
            return InferredType::concrete(args[0].type.dtype, Shape::ndShape({p, 0}));
    }
    // Column-vector operand: repmat(colVec, p, q) tiles an n x 1 column into (p*n) x q.
    //   q == 1  -> a (p*n) x 1 column (p copies stacked), a 1-D column (Unknown shape).
    //   q  > 1  -> a true 2-D (p*n) x q matrix. cols = q (known), rows = p*n (runtime, n is
    //              the operand's runtime length) -> NDims rank-2 ndShape({0, q}). The mirror
    //              of the row case.
    if (args[0].type.shape.kind == ShapeKind::ColVector && args.size() == 3) {
        std::size_t p = 0, q = 0;
        if (args[2].constant.asDim(q) && q == 1)
            return InferredType::concrete(args[0].type.dtype, Shape::unknown());
        if (args[1].constant.asDim(p) && args[2].constant.asDim(q) && q > 1)
            return InferredType::concrete(args[0].type.dtype, Shape::ndShape({0, q}));
    }
    // Matrix operand: repmat(A, p, q) tiles an Arows x Acols matrix into a (p*Arows) x
    // (q*Acols) block grid -> a runtime-dim 2-D (NDims rank-2). v1: the 3-arg form, A a
    // 2-D matrix (KnownDims rows>1 & cols>1, or an NDims rank-2 runtime-dim matrix).
    if (args.size() == 3
        && ((args[0].type.shape.kind == ShapeKind::KnownDims && args[0].type.shape.rows > 1
             && args[0].type.shape.cols > 1)
            || (args[0].type.shape.kind == ShapeKind::NDims && args[0].type.shape.nd.size() == 2)))
        return InferredType::concrete(args[0].type.dtype, Shape::ndShape({0, 0}));
    return InferredType::dynamic();
}

} // namespace

void registerConstructorTransfers(TransferRegistry &reg)
{
    reg.add("zeros", zerosOnesTransfer);
    reg.add("ones", zerosOnesTransfer);
    reg.add("linspace", linspaceTransfer);
    reg.add("logspace", logspaceTransfer);
    reg.add("reshape", reshapeTransfer);  // (x,m,n) -> m x n, same column-major data
    reg.add("repmat", repmatTransfer);    // (s,m,n) scalar -> m x n all = s (v1)
    reg.add("eye", eyeTransfer);          // eye(n)/eye(m,n) -> identity matrix
}

} // namespace numkit::codegen
