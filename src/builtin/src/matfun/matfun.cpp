// src/builtin/src/matfun/matfun.cpp
//
// Matrix functions implementations for numkit::builtin.

#include <numkit/builtin/matfun.hpp>
#include <numkit/value/value.hpp>
#include <numkit/value/span.hpp>
#include <numkit/builtin/ops.hpp>
#include <numkit/builtin/datatypes.hpp>
#include <numkit/builtin/elfun.hpp>

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

    const Value da = numkit::builtin::toDouble(a, mr);
    const Value db = numkit::builtin::toDouble(b, mr);
    Value q = numkit::builtin::rdivide(da, db, mr);

    if (opt == "fix")          q = numkit::builtin::fix(q, mr);
    else if (opt == "floor")  q = numkit::builtin::floor(q, mr);
    else if (opt == "ceil")   q = numkit::builtin::ceil(q, mr);
    else if (opt == "round")  q = numkit::builtin::round(q, mr);
    else
        throw std::runtime_error("idivide: opt must be 'fix', 'floor', 'ceil', or 'round'");

    return numkit::builtin::cast(q, mtypeName(resultType), mr);
}

} // namespace numkit::builtin
