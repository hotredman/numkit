// src/bundle/src/register/builtin/matfun_reg.cpp

#include <nk/builtin/matfun.hpp>
#include <nk/core/engine.hpp>
#include <nk/value/value.hpp>
#include <nk/value/span.hpp>

#include <stdexcept>
#include <string>

namespace nk::bundle::builtin {

void register_matfun(Engine &engine) {
    engine.registerFunction("idivide",
        [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            if (args.size() < 2)
                throw std::runtime_error("idivide requires (A, B[, opt])");
            std::string opt = "fix";
            if (args.size() >= 3 && (args[2].isChar() || args[2].isString())) {
                opt = args[2].toString();
            }
            outs[0] = nk::builtin::idivide(args[0], args[1], opt, ctx.engine->resource());
        });
}

} // namespace nk::bundle::builtin
