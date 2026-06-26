// codegen/src/transfer/registry.cpp
//
// The TransferRegistry and the top-level registerStandardTransfers()
// aggregator (calls each per-family registrar).

#include <numkit/codegen/transfer.hpp>

#include <utility>

namespace numkit::codegen {

void TransferRegistry::add(std::string name, TransferFn fn)
{
    table_.insert_or_assign(std::move(name), std::move(fn));
}

void TransferRegistry::addMulti(std::string name, MultiTransferFn fn)
{
    multiTable_.insert_or_assign(std::move(name), std::move(fn));
}

std::vector<InferredType> TransferRegistry::applyMulti(const std::string &name,
                                                       const std::vector<ArgInfo> &args) const
{
    // Bottom is ABSORBING in a transfer (see apply): a ⊥ operand carries no value,
    // so the outputs are unknown. Return "no info" (empty) -> the caller defaults
    // each output to Dynamic (sound; multi-output recursion is not fixpoint-refined
    // in v1 -- single-output recursion converges via apply's ⊥ propagation).
    for (const ArgInfo &a : args)
        if (a.type.isBottom()) return {};
    const auto it = multiTable_.find(name);
    if (it == multiTable_.end()) return {};  // no multi-output transfer
    return it->second(args);
}

bool TransferRegistry::has(const std::string &name) const
{
    return table_.find(name) != table_.end();
}

std::size_t TransferRegistry::size() const
{
    return table_.size();
}

InferredType TransferRegistry::apply(const std::string &name,
                                     const std::vector<ArgInfo> &args) const
{
    // Bottom is ABSORBING in a transfer: an operand with no value yet (⊥ -- e.g.
    // the seed of a recursive self-call before its return type is known) yields no
    // value, so the result is ⊥ too. Short-circuit before the per-fn transfer so
    // every op / builtin propagates ⊥ uniformly. This is what lets the recursion
    // return-type fixpoint converge (⊥ seed -> body -> join collapses ⊥ to the
    // base-case type). Dormant until something produces a ⊥ operand (the fixpoint
    // seed); `n * ⊥` -> ⊥, not Dynamic. (⊥ is join's identity but a transfer's
    // absorber -- the dual roles, both sound.)
    for (const ArgInfo &a : args)
        if (a.type.isBottom()) return InferredType::bottom();
    const auto it = table_.find(name);
    if (it == table_.end())
        return InferredType::dynamic();  // unknown builtin -> boxed (sound)
    return it->second(args);
}

void registerStandardTransfers(TransferRegistry &reg)
{
    registerConstructorTransfers(reg);
    registerElementwiseTransfers(reg);
    registerShapeTransfers(reg);
    // future: registerReductionTransfers(reg);
    //         registerCastTransfers(reg);
    //         registerBespokeTransfers(reg);
}

} // namespace numkit::codegen
