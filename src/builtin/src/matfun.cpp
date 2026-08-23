// src/builtin/src/matfun.cpp
//
// Matrix functions implementations and registrations.
#include <numkit/builtin/matfun.hpp>
#include <numkit/core/engine.hpp>
#include <numkit/core/types.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>
#include <numkit/lang/operators/binary_ops.hpp>
#include <numkit/lang/types/types.hpp>
#include <numkit/math/arithmetic/rounding.hpp>

#include <cctype>
#include <stdexcept>
#include <string>

namespace numkit::builtin {

Value idivide(const Value &a, const Value &b, const std::string &mode, std::pmr::memory_resource *mr)
{
    std::string opt = mode;
    for (auto &c : opt)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (opt.empty()) opt = "fix";

    const ValueType t0 = a.type();
    const ValueType t1 = b.type();
    const bool int0 = isIntegerType(t0);
    const bool int1 = isIntegerType(t1);
    const bool dbl0 = (t0 == ValueType::DOUBLE);
    const bool dbl1 = (t1 == ValueType::DOUBLE);

    if (!int0 && !int1)
        throw std::runtime_error("At least one argument must belong to an integer class.");

    ValueType resultType;
    if (int0 && int1) {
        if (t0 != t1)
            throw std::runtime_error("Integers can only be combined with integers of the same class, or scalar doubles.");
        resultType = t0;
    } else if (int0) {
        if (!dbl1 || !b.isScalar())
            throw std::runtime_error("Integers can only be combined with integers of the same class, or scalar doubles.");
        resultType = t0;
    } else {
        if (!dbl0 || !a.isScalar())
            throw std::runtime_error("Integers can only be combined with integers of the same class, or scalar doubles.");
        resultType = t1;
    }

    const Value da = numkit::lang::toDouble(a, mr);
    const Value db = numkit::lang::toDouble(b, mr);
    Value q = numkit::lang::rdivide(da, db, mr);

    if (opt == "fix")          q = numkit::math::fix(q, mr);
    else if (opt == "floor")  q = numkit::math::floor(q, mr);
    else if (opt == "ceil")   q = numkit::math::ceil(q, mr);
    else if (opt == "round")  q = numkit::math::round(q, mr);
    else
        throw std::runtime_error("idivide: opt must be 'fix', 'floor', 'ceil', or 'round'");

    return numkit::lang::cast(q, mtypeName(resultType), mr);
}

void register_matfun(Engine &engine) {
    engine.registerFunction("idivide",
        [](Span<const Value> args, size_t, Span<Value> outs, CallContext &ctx) {
            if (args.size() < 2)
                throw std::runtime_error("idivide requires (A, B[, opt])");
            std::string opt = "fix";
            if (args.size() >= 3 && (args[2].isChar() || args[2].isString())) {
                opt = args[2].toString();
            }
            outs[0] = numkit::builtin::idivide(args[0], args[1], opt, ctx.engine->resource());
        });
}

} // namespace numkit::builtin
