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
    } else if (args.size() == 2) {
        sh = (args[0].constant.asDim(a) && args[1].constant.asDim(b))
                 ? Shape::dims(a, b)
                 : Shape::unknown();
    } else {
        // >= 3 dims: a ranked N-D array of rank = nargs. Each known-constant
        // dim records its exact size; a runtime dim records 0 (unknown). The
        // shape stays NDims (ranked) rather than collapsing to Unknown — sound
        // (it over-approximates an unknown dim), and it lets the emitter
        // materialise runtime dim vars from the call args (a runtime-dim N-D
        // local). ndShape canonicalises a fully-known rank-2 back to KnownDims.
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

} // namespace

void registerConstructorTransfers(TransferRegistry &reg)
{
    reg.add("zeros", zerosOnesTransfer);
    reg.add("ones", zerosOnesTransfer);
    reg.add("linspace", linspaceTransfer);
    reg.add("logspace", logspaceTransfer);
}

} // namespace numkit::codegen
