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
    const auto it = table_.find(name);
    if (it == table_.end())
        return InferredType::dynamic();  // unknown builtin -> boxed (sound)
    return it->second(args);
}

void registerStandardTransfers(TransferRegistry &reg)
{
    registerConstructorTransfers(reg);
    // future: registerElementwiseTransfers(reg);
    //         registerReductionTransfers(reg);
    //         registerShapeTransfers(reg);
    //         registerCastTransfers(reg);
    //         registerBespokeTransfers(reg);
}

} // namespace numkit::codegen
