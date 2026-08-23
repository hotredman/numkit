// src/runtime/src/language/splitapply_callback.cpp
//
// VM State-machine splitapply callback implementation.

#include <numkit/core/callback_builtin.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/core/vm.hpp>
#include <numkit/value/value.hpp>

#include <map>
#include <memory>
#include <vector>

namespace numkit::runtime {

namespace detail {

struct SplitapplyCallbackBuiltin : CallbackBuiltin
{
    std::shared_ptr<VmContinuation> tryStart(Span<const Value> args, std::size_t nargout,
                                             Value *dest, Engine &eng) override
    {
        if (args.size() < 3 || nargout > 1)
            return nullptr;
        if (!eng.isUserCodeHandle(args[0]))
            return nullptr;
        const Value &G = args[args.size() - 1];
        const std::size_t nIn = args.size() - 2;
        if (nIn == 0)
            return nullptr;
        const std::size_t n = G.numel();
        for (std::size_t k = 0; k < nIn; ++k)
            if (args[1 + k].numel() != n)
                return nullptr; // shape error -> synchronous path reports it
        auto *mr = eng.resource();
        // Bucket indices by group id (sorted).
        std::map<int, std::vector<std::size_t>> buckets;
        for (std::size_t i = 0; i < n; ++i)
            buckets[(int)G.elemAsDouble(i)].push_back(i);
        auto groups = std::make_shared<std::vector<std::vector<std::size_t>>>();
        for (auto &kv : buckets)
            groups->push_back(std::move(kv.second));
        auto inputs = std::make_shared<std::vector<Value>>();
        for (std::size_t k = 0; k < nIn; ++k)
            inputs->push_back(args[1 + k]);
        auto cont = std::make_shared<LoopContinuation>();
        cont->handle = args[0];
        cont->n = groups->size();
        cont->dest = dest;
        cont->makeArgs = [inputs, groups, mr](std::size_t i) -> std::vector<Value> {
            const auto &idxs = (*groups)[i];
            std::vector<Value> callArgs(inputs->size());
            for (std::size_t k = 0; k < inputs->size(); ++k) {
                auto sub = Value::matrix(idxs.size(), 1, ValueType::DOUBLE, mr);
                double *sd = sub.doubleDataMut();
                for (std::size_t j = 0; j < idxs.size(); ++j)
                    sd[j] = (*inputs)[k].elemAsDouble(idxs[j]);
                callArgs[k] = std::move(sub);
            }
            return callArgs;
        };
        cont->pack = [mr](std::vector<Value> &results) -> Value {
            auto out = Value::matrix(results.size(), 1, ValueType::DOUBLE, mr);
            double *d = out.doubleDataMut();
            for (std::size_t i = 0; i < results.size(); ++i)
                d[i] = results[i].toScalar();
            return out;
        };
        cont->results.reserve(cont->n);
        return cont;
    }
};

} // namespace detail

void registerSplitapplyCallbackBuiltin(Engine &engine)
{
    engine.registerCallbackBuiltin("splitapply",
                                   std::make_shared<detail::SplitapplyCallbackBuiltin>());
}

} // namespace numkit::runtime
