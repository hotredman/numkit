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
// 1xNdims row vector — deferred (-> Dynamic, the sound fallback).
InferredType sizeTransfer(const std::vector<ArgInfo> &args)
{
    return args.size() == 2 ? InferredType::scalar(ValueType::DOUBLE) : InferredType::dynamic();
}

} // namespace

void registerShapeTransfers(TransferRegistry &reg)
{
    reg.add("numel", countTransfer);
    reg.add("length", countTransfer);
    reg.add("ndims", countTransfer);
    reg.add("size", sizeTransfer);
}

} // namespace numkit::codegen
